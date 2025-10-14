// Terminal-only menu CLI for llvm_obfuscation.
// Single step-based flow (screenshot-style):
//  1) File selection
//  2) Numbered preset selector (default 2)
//  3) Processing (progress bar)
//  4) Result summary + prompt
// No ncurses dependency; uses ANSI for coloring where available.

#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace myfs {
    using path = std::string;
    inline bool create_directories(const path &p) {
        if (p.empty()) return true;
        std::string accum;
        for (size_t i = 0; i < p.size(); ++i) {
            accum.push_back(p[i]);
            if (p[i] == '/' || i + 1 == p.size()) {
                if (accum.empty()) continue;
                struct stat st;
                if (stat(accum.c_str(), &st) != 0) {
                    if (mkdir(accum.c_str(), 0755) != 0) {
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

static bool supports_color() {
    const char* term = std::getenv("TERM");
    return term && std::string(term) != "dumb";
}

static const char* C_RESET = "\x1b[0m";
static const char* C_BOLD = "\x1b[1m";
static const char* C_CYAN = "\x1b[36m";
static const char* C_GREEN = "\x1b[32m";
static const char* C_YELLOW = "\x1b[33m";
static const char* C_MAGENTA = "\x1b[35m";

static int terminal_width() {
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return w.ws_col;
    return 80;
}

static void center_print(const std::string &s, const char *color=nullptr) {
    int w = terminal_width();
    int pad = std::max(0, (w - (int)s.size())/2);
    std::cout << std::string(pad, ' ');
    if (color && supports_color()) std::cout << color << s << C_RESET << "\n";
    else std::cout << s << "\n";
}

static uint32_t choose_seed(uint32_t provided) {
    if (provided != 0) return provided;
    std::srand((unsigned)std::time(nullptr));
    return ((uint32_t)std::rand() << 16) ^ (uint32_t)std::rand();
}

static bool run_cmd(const std::string &cmd) {
    std::cout << "[RUN] " << cmd << "\n";
    int rc = std::system(cmd.c_str());
    if (rc != 0) std::cerr << "[ERR] Command failed: " << rc << "\n";
    return rc == 0;
}

static bool ensure_dirs(const RunConfig &cfg) {
    bool a = myfs::create_directories(cfg.workdir);
    bool b = myfs::create_directories(myfs::parent_path(cfg.out_bin));
    return a && b;
}

// Keep the pipeline implementation from previous file - it uses system clang/opt/llc
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

        std::string passes;
        // Map difficulty/preset to sets of obfuscation passes
        if (cfg.preset == "light") {
            passes = "string-obf";
        } else if (cfg.preset == "balanced") {
            // balanced includes string obf + some bogus insert and fake loops
            passes = "string-obf,bogus-insert,fake-loop";
        } else {
            // aggressive: everything
            passes = "string-obf,bogus-insert,fake-loop,control-flow-flattening";
        }

    std::ostringstream envs;
    envs << "LLVM_OBF_SEED=" << cfg.seed << " ";
    envs << "LLVM_OBF_BOGUS_RATIO=" << cfg.bogus_ratio << " ";
    envs << "LLVM_OBF_STRING_INTENSITY=" << cfg.string_intensity << " ";
    envs << "LLVM_OBF_CYCLES=" << cfg.cycles << " ";

    const std::string counters_path = cfg.workdir + "/counters.json";
    envs << "OFILE=" << counters_path << " ";

    std::string plugin_path = "build/libObfPasses.so";
#ifdef _WIN32
    plugin_path = "build/libObfPasses.dll";
#endif

    std::ostringstream optcmd;
    optcmd << envs.str() << "opt -load-pass-plugin=" << plugin_path
           << " -passes=" << passes << " " << bc << " -o " << obf_bc;

    bool opt_ok = run_cmd(optcmd.str());
    if (!opt_ok) {
        std::ostringstream alt;
        alt << envs.str() << "opt -load " << plugin_path << " -string-obf " << bc << " -o " << obf_bc;
        if (!run_cmd(alt.str())) {
            std::cerr << "opt failed (attempted both -load-pass-plugin and -load)\n";
            return false;
        }
    }

    std::ostringstream llc_cmd;
    llc_cmd << "llc -filetype=obj " << obf_bc << " -o " << obj;
    if (!run_cmd(llc_cmd.str())) return false;

    std::ostringstream link_cmd;
#ifdef _WIN32
    link_cmd << "clang " << obj << " src/runtime/decryptor.c -static -o " << cfg.out_bin << ".exe";
#else
    link_cmd << "clang " << obj << " src/runtime/decryptor.c -o " << cfg.out_bin;
#endif
    if (!run_cmd(link_cmd.str())) return false;

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

static void print_header() {
    int w = terminal_width();
    std::string sep(w - 4, '=');
    if (supports_color()) std::cout << C_CYAN;
    std::cout << "  " << sep << "\n";
    center_print("LLVM CODE OBFUSCATOR", supports_color() ? C_BOLD : nullptr);
    center_print("Advanced Code Protection Suite", supports_color() ? C_MAGENTA : nullptr);
    std::cout << "  " << sep << "\n";
    if (supports_color()) std::cout << C_RESET;
}

static int numbered_selector(const std::vector<std::pair<int,std::string>> &items, int default_choice) {
    for (const auto &p : items) {
        if (supports_color()) std::cout << C_MAGENTA << p.first << ". " << C_RESET;
        else std::cout << p.first << ". ";
        std::cout << p.second << "\n";
    }
    if (supports_color()) std::cout << C_YELLOW;
    std::cout << "[G] Select preset [" << default_choice << "]: ";
    if (supports_color()) std::cout << C_RESET;
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return default_choice;
    int c = std::atoi(line.c_str());
    for (auto &p: items) if (p.first == c) return c;
    return default_choice;
}

static void progress_simulate() {
    const int width = terminal_width();
    const int bar_w = std::min(60, width - 40);
    for (int pct = 0; pct <= 100; pct += 4) {
        int filled = (pct * bar_w) / 100;
        std::ostringstream ss;
        ss << "    [";
        for (int i = 0; i < bar_w; ++i) ss << (i < filled ? '#' : ' ');
        ss << "] " << std::setw(3) << pct << "%";
        std::cout << "\r" << ss.str() << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    std::cout << "\n";
}

static void show_summary(const RunConfig &cfg, bool success) {
    if (supports_color()) std::cout << C_CYAN;
    std::cout << "\n==================== OBFUSCATION SUMMARY ====================\n";
    if (supports_color()) std::cout << C_RESET;
    std::cout << "  Input file  : " << cfg.src << "\n";
    std::cout << "  Output file : " << cfg.out_bin << "\n";
    std::cout << "  Preset      : " << cfg.preset << "\n";
    std::cout << "  Cycles      : " << cfg.cycles << "\n";
    std::cout << "  Bogus%      : " << cfg.bogus_ratio << "%\n";
    if (success) {
        if (supports_color()) std::cout << C_GREEN;
        std::cout << "\n  [SUCCESS] Obfuscation completed successfully!\n";
        if (supports_color()) std::cout << C_RESET;
    } else {
        if (supports_color()) std::cout << C_YELLOW;
        std::cout << "\n  [FAILED] Obfuscation failed. Check the logs above.\n";
        if (supports_color()) std::cout << C_RESET;
    }
}

int main() {
    RunConfig cfg;
    bool again = true;
    while (again) {
        print_header();

        // STEP 1: File selection
        std::cout << "\n=> STEP 1: File Selection & Analysis =>\n";
        std::cout << "Enter path to source file (leave empty for '" << cfg.src << "'): ";
        std::string in; std::getline(std::cin, in);
        if (!in.empty()) cfg.src = in;

        // STEP 2: Preset selector
        std::cout << "\n=> STEP 2: Preset Selection =>\n";
        std::vector<std::pair<int,std::string>> presets = {
            {1, "Light Protection - Fast, minimal obfuscation"},
            {2, "Balanced Protection - Good security/speed ratio"},
            {3, "Maximum Protection - Maximum security"},
            {4, "Custom Configuration - Manual settings"},
            {5, "Exit Program"}
        };
        int sel = numbered_selector(presets, 2);
        if (sel == 5) break;
        if (sel == 1) {
            cfg.preset = "light";
            // Light: minimal obfuscation
            cfg.bogus_ratio = 5;
            cfg.cycles = 1;
            cfg.string_intensity = 1;
        } else if (sel == 2) {
            cfg.preset = "balanced";
            // Balanced: reasonable obfuscation
            cfg.bogus_ratio = 20;
            cfg.cycles = 2;
            cfg.string_intensity = 2;
        } else if (sel == 3) {
            cfg.preset = "aggressive";
            // Aggressive: heavy obfuscation
            cfg.bogus_ratio = 45;
            cfg.cycles = 4;
            cfg.string_intensity = 3;
        } else {
            cfg.preset = "custom";
            // Prompt user for manual values
            std::cout << "\n-- Custom configuration --\n";
            std::cout << "Bogus ratio (0-100) [" << cfg.bogus_ratio << "]: ";
            std::string brs; std::getline(std::cin, brs);
            if (!brs.empty()) {
                int br = std::atoi(brs.c_str()); if (br < 0) br = 0; if (br > 100) br = 100; cfg.bogus_ratio = br;
            }
            std::cout << "Cycles (number of obfuscation rounds) [" << cfg.cycles << "]: ";
            std::string cs; std::getline(std::cin, cs);
            if (!cs.empty()) { int c = std::atoi(cs.c_str()); if (c < 1) c = 1; cfg.cycles = c; }
            std::cout << "String intensity (1=low,2=med,3=high) [" << cfg.string_intensity << "]: ";
            std::string sis; std::getline(std::cin, sis);
            if (!sis.empty()) { int si = std::atoi(sis.c_str()); if (si < 1) si = 1; cfg.string_intensity = si; }
            std::cout << "Output binary path (leave empty for '" << cfg.out_bin << "'): ";
            std::string outp; std::getline(std::cin, outp);
            if (!outp.empty()) cfg.out_bin = outp;
        }

        // STEP 3: Processing
        std::cout << "\n=> STEP 3: Processing =>\n";
        progress_simulate();

        // Now run the actual pipeline (may print additional output)
        cfg.seed = choose_seed(cfg.seed);
        std::cout << "[INFO] Using seed: " << cfg.seed << "\n";
        std::string report;
        bool ok = run_pipeline(cfg, report);

        // STEP 4: Summary
        show_summary(cfg, ok);
        std::cout << "\nProcess another file [y/N]: ";
        std::string yn; std::getline(std::cin, yn);
        if (yn.empty() || (yn[0] != 'y' && yn[0] != 'Y')) again = false;
    }
    std::cout << "Goodbye\n";
    return 0;
}
