// src/driver/menu_cli.cpp
// Simple menu-driven CLI for llvm_obfuscation (text UI).
// - Allows selecting preset, seed, bogus ratio, cycles, string obf count.
// - Produces a JSON report and builds the obfuscated binary by invoking
//   the programmatic pipeline (via system clang/llc calls or by calling
//   driver functions if you integrate).
//
// Note: this file assumes clang/llc available on PATH and build/ directory exists.

#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
// Minimal, portable filesystem compatibility used by the menu UI. We avoid
// depending on <filesystem> to prevent IntelliSense/analysis from complaining
// on systems or configs that don't expose std::filesystem.
#include <sys/stat.h>
#include <sys/types.h>
namespace myfs {
    using path = std::string;
    inline bool create_directories(const path &p) {
        if (p.empty()) return true;
        // naive recursive mkdir -p like implementation
        std::string accum;
        for (size_t i = 0; i < p.size(); ++i) {
            accum.push_back(p[i]);
            if (p[i] == '/' || i + 1 == p.size()) {
                if (accum.empty()) continue;
                struct stat st;
                if (stat(accum.c_str(), &st) != 0) {
                    if (mkdir(accum.c_str(), 0755) != 0) {
                        // if creation failed and directory still doesn't exist -> fail
                        if (stat(accum.c_str(), &st) != 0) return false;
                    }
                }
            }
        }
        return true;
    }
    inline path parent_path(const path &p) {
        size_t pos = p.find_last_of("/\\");
        if (pos == std::string::npos) return std::string();
        return p.substr(0, pos);
    }
}

struct RunConfig {
    std::string src = "tests/hello.c";
    std::string preset = "balanced"; // light | balanced | aggressive
    uint32_t seed = 0;
    int bogus_ratio = 20;    // percentage 0-100
    int string_intensity = 1; // multiplier
    int cycles = 1;
    std::string out_bin = "dist/main_obf";
    std::string workdir = "build";
};

static std::string now_iso8601() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}
// Write a small JSON config for the run (for reproducibility & reporting)
static bool write_run_json(const RunConfig &cfg, const std::string &json_path) {
    std::ofstream os(json_path);
    if (!os) return false;
    os << "{\n";
    os << "  \"timestamp\": \"" << now_iso8601() << "\",\n";
    os << "  \"src\": \"" << cfg.src << "\",\n";
    os << "  \"preset\": \"" << cfg.preset << "\",\n";
    os << "  \"seed\": " << cfg.seed << ",\n";
    os << "  \"bogus_ratio\": " << cfg.bogus_ratio << ",\n";
    os << "  \"string_intensity\": " << cfg.string_intensity << ",\n";
    os << "  \"cycles\": " << cfg.cycles << "\n";
    os << "}\n";
    os.close();
    return true;
}

// The core pipeline using system commands (clang -> opt -> llc -> clang link).
// This is intentionally simple and robust cross-platform (assuming clang/opt/llc exist).
static bool run_pipeline(const RunConfig &cfg, std::string &report_json_out) {
    if (!ensure_dirs(cfg)) {
        std::cerr << "Failed to create build/output directories\n";
        return false;
    }

    const std::string bc = cfg.workdir + "/main.bc";
    const std::string obf_bc = cfg.workdir + "/main_obf.bc";
    const std::string obj = cfg.workdir + "/main_obf.o";

    // 1) compile source -> bitcode
    std::ostringstream cmd;
    cmd << "clang -emit-llvm -c -g -O0 " << cfg.src << " -o " << bc;
    if (!run_cmd(cmd.str())) return false;

    // 2) build opt pipeline string (uses textual pipeline names)
    // Map our user config to passes/preset string
    std::string passes;
    if (cfg.preset == "light") {
        passes = "string-obf";
    } else if (cfg.preset == "balanced") {
        passes = "string-obf,bogus-insert";
    } else {
        // aggressive
        passes = "string-obf,bogus-insert,control-flow-flattening";
    }
    // allow bogus_ratio and cycles to be exported via environment vars used by passes
    std::ostringstream envs;
    envs << "LLVM_OBF_SEED=" << cfg.seed << " ";
    envs << "LLVM_OBF_BOGUS_RATIO=" << cfg.bogus_ratio << " ";
    envs << "LLVM_OBF_STRING_INTENSITY=" << cfg.string_intensity << " ";
    envs << "LLVM_OBF_CYCLES=" << cfg.cycles << " ";

    // write OFILE path for passes to write counters.json
    const std::string counters_path = cfg.workdir + "/counters.json";
    envs << "OFILE=" << counters_path << " ";

    // 3) run opt with plugin (we expect build/libObfPasses.so or the integrated exe)
    // Prefer plugin if exists
    std::string plugin_path = "build/libObfPasses.so";
#ifdef _WIN32
    plugin_path = "build/libObfPasses.dll";
#endif

    std::ostringstream optcmd;
    optcmd << envs.str() << "opt -load-pass-plugin=" << plugin_path
           << " -passes=" << passes << " " << bc << " -o " << obf_bc;

    // fallback for older plugin style if needed (opt -load)
    bool opt_ok = run_cmd(optcmd.str());
    if (!opt_ok) {
        std::ostringstream alt;
        alt << envs.str() << "opt -load " << plugin_path << " -string-obf " << bc << " -o " << obf_bc;
        if (!run_cmd(alt.str())) {
            std::cerr << "opt failed (attempted both -load-pass-plugin and -load)\n";
            return false;
        }
    }

    // 4) llc -> object
    std::ostringstream llc_cmd;
    llc_cmd << "llc -filetype=obj " << obf_bc << " -o " << obj;
    if (!run_cmd(llc_cmd.str())) return false;

    // 5) link with runtime
    std::ostringstream link_cmd;
#ifdef _WIN32
    link_cmd << "clang " << obj << " src/runtime/decryptor.c -static -o " << cfg.out_bin << ".exe";
#else
    link_cmd << "clang " << obj << " src/runtime/decryptor.c -o " << cfg.out_bin;
#endif
    if (!run_cmd(link_cmd.str())) return false;

    // 6) read counters.json (if created) and create a final report JSON combining run config and counters
    std::ostringstream final_json;
    final_json << cfg.workdir << "/run_report.json";
    report_json_out = final_json.str();

    std::ifstream counters(counters_path);
    std::string counters_data;
    if (counters) {
        std::ostringstream ss;
        ss << counters.rdbuf();
        counters_data = ss.str();
    }
    // create combined JSON
    std::ofstream out(report_json_out);
    out << "{\n";
    out << "  \"run_config\": {\n";
    out << "    \"src\": \"" << cfg.src << "\",\n";
    out << "    \"preset\": \"" << cfg.preset << "\",\n";
    out << "    \"seed\": " << cfg.seed << ",\n";
    out << "    \"bogus_ratio\": " << cfg.bogus_ratio << ",\n";
    out << "    \"string_intensity\": " << cfg.string_intensity << ",\n";
    out << "    \"cycles\": " << cfg.cycles << "\n";
    out << "  },\n";
    out << "  \"counters\": " << (counters_data.empty() ? "{}" : counters_data) << "\n";
    out << "}\n";
    out.close();

    std::cout << "[OK] Built '" << cfg.out_bin << "'\n";
    std::cout << "[OK] Report: " << report_json_out << "\n";
    return true;
}

int main() {
    RunConfig cfg;
    cfg.seed = 0; // 0 -> auto choose
    bool exit_flag = false;
    while (!exit_flag) {
        show_header();
        show_config(cfg);
        std::cout << "Menu:\n";
        std::cout << "  1) Change source file\n";
        std::cout << "  2) Select preset (light / balanced / aggressive)\n";
        std::cout << "  3) Set seed (0=random)\n";
        std::cout << "  4) Set bogus ratio (0-100)\n";
        std::cout << "  5) Set string intensity (1..)\n";
        std::cout << "  6) Set cycles\n";
        std::cout << "  7) Run obfuscation\n";
        std::cout << "  8) Export last report to HTML (if exists)\n";
        std::cout << "  9) Quit\n";
        std::cout << "\nSelect option: ";
        int opt; std::cin >> opt;
        switch (opt) {
            case 1: {
                std::cout << "Enter source file path: ";
                std::string s; std::cin >> s; cfg.src = s;
                break;
            }
            case 2: {
                std::cout << "Preset (light/balanced/aggressive): ";
                std::string p; std::cin >> p; cfg.preset = p;
                break;
            }
            case 3: {
                std::cout << "Seed (0 for random): ";
                uint32_t sd; std::cin >> sd; cfg.seed = sd;
                break;
            }
            case 4: {
                std::cout << "Bogus ratio (0-100): ";
                int br; std::cin >> br; if (br < 0) br = 0; if (br>100) br=100; cfg.bogus_ratio = br;
                break;
            }
            case 5: {
                std::cout << "String intensity (1..): ";
                int si; std::cin >> si; if (si<1) si = 1; cfg.string_intensity = si;
                break;
            }
            case 6: {
                std::cout << "Cycles: ";
                int c; std::cin >> c; if (c < 1) c = 1; cfg.cycles = c;
                break;
            }
            case 7: {
                // finalize seed
                cfg.seed = choose_seed(cfg.seed);
                std::cout << "[INFO] Using seed: " << cfg.seed << "\n";
                std::string report;
                if (run_pipeline(cfg, report)) {
                    std::cout << "[INFO] Run finished. Report: " << report << "\n";
                } else {
                    std::cerr << "[ERR] Run failed\n";
                }
                std::cout << "Press Enter to continue...";
                std::string dummy; std::getline(std::cin, dummy); std::getline(std::cin, dummy);
                break;
            }
            case 8: {
                // export last run report to HTML
                std::string json_in = cfg.workdir + "/run_report.json";
                std::ifstream in(json_in);
                if (!in) {
                    std::cerr << "No report found at " << json_in << "\n";
                    break;
                }
                std::ostringstream ss; ss << in.rdbuf();
                std::string json = ss.str();
                std::string out_html = cfg.workdir + "/report.html";
                std::ofstream os(out_html);
                os << "<!doctype html><html><head><meta charset='utf-8'><title>Obfuscation Report</title></head><body>";
                os << "<h1>Obfuscation Report</h1><pre>" << json << "</pre></body></html>";
                os.close();
                std::cout << "Written HTML: " << out_html << "\n";
                std::cout << "Press Enter to continue...";
                std::string d; std::getline(std::cin, d); std::getline(std::cin, d);
                break;
            }
            case 9:
                exit_flag = true;
                break;
            default:
                std::cout << "Unknown option\n";
                break;
        }
    }
    std::cout << "Goodbye\n";
    return 0;
}
