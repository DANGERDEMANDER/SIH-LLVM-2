#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib> // For system()
#include <fstream>
#include <sstream>
#include <map>
#include <ctime>   // For std::time and std::ctime
#include <random>  // For std::mt19937
#include <limits>  // Required for std::numeric_limits
#include <filesystem> // For getting absolute paths and file size
#include <iomanip> // For std::setprecision

// --- UI Components ---
#ifdef _WIN32
#define CLEAR_SCREEN "cls"
#else
#define CLEAR_SCREEN "clear"
#endif

namespace Color {
    const std::string RESET = "\033[0m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string RED = "\033[31m";
    const std::string CYAN = "\033[36m";
    const std::string BOLD = "\033[1m";
}

void printHeader(const std::string& title) {
    (void)system(CLEAR_SCREEN);
    std::cout << Color::BOLD << Color::GREEN;
    std::cout << "=========================================================\n";
    std::cout << "                " << title << "                \n";
    std::cout << "    Professional Security & Anti-Analysis      \n";
    std::cout << "=========================================================\n\n" << Color::RESET;
}

void printStep(const std::string& step) {
    std::cout << Color::BOLD << Color::YELLOW << ">> " << step << "\n" << Color::RESET;
    std::cout << "---------------------------------------------------------\n";
}

void printSuccess(const std::string& message) {
    std::cout << Color::BOLD << Color::GREEN << "[SUCCESS] " << Color::RESET << message << "\n";
}

void printError(const std::string& message) {
    std::cerr << Color::BOLD << Color::RED << "[ERROR] " << Color::RESET << message << "\n";
}

void printInfo(const std::string& key, const std::string& value) {
    std::cout << Color::BOLD << Color::CYAN << std::left << std::setw(30) << key << ": " << Color::RESET << value << "\n";
}

void progressBar(int percentage, const std::string& message) {
    int barWidth = 40;
    std::cout << "[";
    int pos = barWidth * percentage / 100;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << percentage << "% - " << message << "\r";
    std::cout.flush();
}

// --- Obfuscation Configuration & Results ---
struct ObfuscationConfig {
    bool stringObfuscation = false;
    bool bogusControlFlow = false;
    bool controlFlowFlattening = false;
    bool fakeLoops = false;
    int stringObfCycles = 1;
    int bogusControlFlowCycles = 1;
    int bogusControlFlowRatio = 30;
    int flatteningCycles = 1;
    int fakeLoopCycles = 1;
    uint32_t seed = 0;
    std::string presetName = "Light";
};

struct ObfuscationResult {
    bool success = false;
    std::map<std::string, long long> stats;
    std::map<std::string, long long> initialAnalysis;
    std::map<std::string, long long> finalAnalysis;
};

// --- Robust Input & Command Execution ---
int getIntegerInput() {
    int value;
    while (!(std::cin >> value)) {
        printError("Invalid input. Please enter a number.");
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "> " << Color::BOLD;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << Color::RESET;
    return value;
}

void parseAndUpdateStats(const std::string& jsonPath, std::map<std::string, long long>& statsMap) {
    std::ifstream jsonFile(jsonPath);
    if (!jsonFile.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(jsonFile)), std::istreambuf_iterator<char>());
    auto find_and_parse = [&](const std::string& key, const std::string& map_key) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos != std::string::npos) {
            size_t colon_pos = content.find(':', pos);
            if (colon_pos != std::string::npos) {
                try {
                    statsMap[map_key] += std::stoll(content.substr(colon_pos + 1));
                } catch (...) { /* ignore parse errors */ }
            }
        }
    };
    find_and_parse("num_strings_encrypted", "Encrypted Strings");
    find_and_parse("total_string_bytes", "Encrypted String Bytes");
}


bool runCommand(const std::string& command, const std::string& statsFile, std::map<std::string, long long>& passStats, std::map<std::string, long long>& analysisStats) {
    const std::string ERR_LOG = "error.log";
    std::string fullCommand = command + " 2> " + ERR_LOG;
    int result = system(fullCommand.c_str());

    std::ifstream errFile(ERR_LOG);
    if(errFile.is_open()) {
        std::string line;
        while (std::getline(errFile, line)) {
            if (line.find("Number of instructions") != std::string::npos || line.find("Number of basic blocks") != std::string::npos) {
                std::stringstream ss(line);
                long long val;
                if (ss >> val) {
                    if (line.find("Number of instructions") != std::string::npos) {
                        analysisStats["Instruction Count"] = val;
                    } else if (line.find("Number of basic blocks") != std::string::npos) {
                        analysisStats["Basic Block Count"] = val;
                    }
                }
            }
            if (line.find("[BogusInsert] inserted ") != std::string::npos) {
                long long inserts = 0;
                if(sscanf(line.c_str(), "[BogusInsert] inserted %lld inserts", &inserts) == 1) {
                    passStats["Bogus Blocks Inserted"] += inserts;
                }
            }
            else if (line.find("[FakeLoop] inserted ") != std::string::npos) {
                long long loops = 0;
                if(sscanf(line.c_str(), "[FakeLoop] inserted %lld loops", &loops) == 1) {
                    passStats["Fake Loops Added"] += loops;
                }
            }
        }
        errFile.close();
    }
    
    parseAndUpdateStats(statsFile, passStats);

    if (result != 0) {
        std::cerr << Color::BOLD << Color::RED << "\n[DEBUG] Command failed. See details below:" << Color::RESET << std::endl;
        std::ifstream errorFile(ERR_LOG);
        if (errorFile.is_open()) {
            std::cerr << Color::RED << "--- Error Log ---\n" << errorFile.rdbuf() << "-----------------" << Color::RESET << std::endl;
        }
    }
    return result == 0;
}

// --- Main Obfuscation & UI Logic ---
ObfuscationResult performObfuscation(const std::string& inputSourceFile, const std::string& outputExecutableName, bool keepIntermediateFiles, ObfuscationConfig& config) {
    ObfuscationResult result;
    if (config.seed == 0) {
        std::random_device rd;
        config.seed = rd();
        printInfo("Generated Random Seed", std::to_string(config.seed));
    }

    const std::string PLUGIN_PATH = "./build/libObfPasses.so";
    const std::string RUNTIME_SRC = "./src/runtime/decryptor.c";
    const std::string FINAL_IR_FILENAME = "final_readable_ir.ll";
    const std::string CLANG = "clang-14";
    const std::string OPT = "opt-14";

    std::string currentIRFile = "temp_0_initial.ll";
    std::vector<std::string> tempFiles = {currentIRFile};

    printStep("1: Initial Analysis & Compilation");
    progressBar(5, "Compiling to LLVM IR...");
    if (!runCommand(CLANG + " -S -emit-llvm " + inputSourceFile + " -o " + currentIRFile, "", result.stats, result.initialAnalysis)) return result;
    result.initialAnalysis["Code Size (bytes)"] = std::filesystem::file_size(currentIRFile);
    
    progressBar(10, "Analyzing initial IR...");
    runCommand(OPT + " -p=instcount,basicaa -stats -S " + currentIRFile + " -o /dev/null", "", result.stats, result.initialAnalysis);

    int totalSteps = (config.stringObfuscation ? config.stringObfCycles : 0) +
                     (config.bogusControlFlow ? config.bogusControlFlowCycles : 0) +
                     (config.controlFlowFlattening ? config.flatteningCycles : 0) +
                     (config.fakeLoops ? config.fakeLoopCycles : 0);
    if (totalSteps == 0) totalSteps = 1;
    int currentStep = 0;

    std::stringstream envStream;
    envStream << "LLVM_OBF_SEED=" << config.seed << " LLVM_OBF_BOGUS_RATIO=" << config.bogusControlFlowRatio << " ";
    
    printStep("2: Applying Obfuscation Passes");
    auto applyPass = [&](const std::string& name, const std::string& flag, bool enabled, int cycles) -> bool {
        if (!enabled) return true;
        for (int i = 0; i < cycles; ++i) {
            currentStep++;
            progressBar(10 + (80 * currentStep / totalSteps), "Applying " + name + " (" + std::to_string(i + 1) + "/" + std::to_string(cycles) + ")");
            std::string nextIRFile = "temp_" + std::to_string(currentStep) + "_" + flag + ".ll";
            tempFiles.push_back(nextIRFile);
            std::string statsFile = "stats_" + std::to_string(currentStep) + ".json";
            std::string command = envStream.str() + " OFILE=" + statsFile + " " + OPT + " -load-pass-plugin=" + PLUGIN_PATH + " -passes=" + flag + " < " + currentIRFile + " > " + nextIRFile;
            if (!runCommand(command, statsFile, result.stats, result.finalAnalysis)) return false;
            currentIRFile = nextIRFile;
        }
        return true;
    };
    
    if (!applyPass("String Obfuscation", "string-obf", config.stringObfuscation, config.stringObfCycles)) return result;
    if (!applyPass("Bogus Control Flow", "bogus-insert", config.bogusControlFlow, config.bogusControlFlowCycles)) return result;
    if (!applyPass("Fake Loops", "fake-loop", config.fakeLoops, config.fakeLoopCycles)) return result;
    if (!applyPass("Control Flow Flattening", "cff", config.controlFlowFlattening, config.flatteningCycles)) return result;

    printStep("3: Finalizing and Linking");
    progressBar(90, "Saving & analyzing final IR...");
    runCommand("cp " + currentIRFile + " " + FINAL_IR_FILENAME, "", result.stats, result.finalAnalysis);
    result.finalAnalysis["Code Size (bytes)"] = std::filesystem::file_size(FINAL_IR_FILENAME);
    runCommand(OPT + " -p=instcount,basicaa -stats -S " + FINAL_IR_FILENAME + " -o /dev/null", "", result.stats, result.finalAnalysis);

    progressBar(97, "Compiling & linking executable...");
    if (!runCommand(CLANG + " " + FINAL_IR_FILENAME + " " + RUNTIME_SRC + " -o " + outputExecutableName, "", result.stats, result.finalAnalysis)) return result;
    
    if (!keepIntermediateFiles) {
        progressBar(99, "Cleaning up temporary files...");
        for (const auto& file : tempFiles) runCommand("rm -f " + file, "", result.stats, result.finalAnalysis);
        runCommand("rm -f stats_*.json error.log", "", result.stats, result.finalAnalysis);
    }

    progressBar(100, "Obfuscation Complete!");
    result.success = true;
    return result;
}

void displayCurrentSettings(const std::string& inputFile, const ObfuscationConfig& config) {
    printStep("Current Settings");
    printInfo("Input Source File", inputFile);
    printInfo("Obfuscation Preset", config.presetName);
    printInfo("Obfuscation Seed", (config.seed == 0 ? "Random" : std::to_string(config.seed)));
    std::cout << "---------------------------------------------------------\n\n";
}

ObfuscationConfig selectPreset() {
    ObfuscationConfig config;
    printStep("Select Obfuscation Preset");
    std::cout << "  1. " << Color::GREEN << "Light" << Color::RESET << " (String Obfuscation)\n";
    std::cout << "  2. " << Color::YELLOW << "Balanced" << Color::RESET << " (String Obf + Bogus CFG + Fake Loops)\n";
    std::cout << "  3. " << Color::RED << "Heavy" << Color::RESET << " (Intense String Obf + Intense Bogus CFG)\n";
    std::cout << "  4. " << Color::BOLD << Color::RED << "Nightmare" << Color::RESET << " (All passes + CFG Flattening)\n";
    std::cout << "  5. " << Color::CYAN << "Custom" << Color::RESET << " (Fine-tune each pass)\n";
    int choice;
    std::cout << "\nSelect preset [1-5]: " << Color::BOLD;
    choice = getIntegerInput();
    char yn;
    switch (choice) {
        case 1: config.presetName = "Light"; config.stringObfuscation = true; break;
        case 2: config.presetName = "Balanced"; config.stringObfuscation = true; config.bogusControlFlow = true; config.fakeLoops = true; config.bogusControlFlowRatio=30; break;
        case 3: config.presetName = "Heavy"; config.stringObfuscation = true; config.stringObfCycles=2; config.bogusControlFlow = true; config.bogusControlFlowCycles=5; config.bogusControlFlowRatio=60; config.fakeLoops = true; config.fakeLoopCycles = 2; break;
        case 4: config.presetName = "Nightmare"; config.stringObfuscation = true; config.stringObfCycles=2; config.bogusControlFlow = true; config.bogusControlFlowCycles=5; config.controlFlowFlattening = true; config.fakeLoops = true; break;
        case 5:
            config.presetName = "Custom";
            std::cout << "\n--- Custom Settings ---\nEnable String Obfuscation? (y/n): " << Color::BOLD; std::cin >> yn; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); config.stringObfuscation = (yn == 'y' || yn == 'Y'); std::cout << Color::RESET;
            if (config.stringObfuscation) { std::cout << "  Cycles: " << Color::BOLD; config.stringObfCycles = getIntegerInput(); }
            std::cout << "Enable Bogus Control Flow? (y/n): " << Color::BOLD; std::cin >> yn; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); config.bogusControlFlow = (yn == 'y' || yn == 'Y'); std::cout << Color::RESET;
            if (config.bogusControlFlow) { std::cout << "  Cycles: " << Color::BOLD; config.bogusControlFlowCycles = getIntegerInput(); std::cout << "  Injection Ratio (0-100)%: " << Color::BOLD; config.bogusControlFlowRatio = getIntegerInput(); }
            std::cout << "Enable Fake Loops? (y/n): " << Color::BOLD; std::cin >> yn; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); config.fakeLoops = (yn == 'y' || yn == 'Y'); std::cout << Color::RESET;
            if (config.fakeLoops) { std::cout << "  Cycles: " << Color::BOLD; config.fakeLoopCycles = getIntegerInput(); }
            std::cout << "Enable Control Flow Flattening? (y/n): " << Color::BOLD; std::cin >> yn; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); config.controlFlowFlattening = (yn == 'y' || yn == 'Y'); std::cout << Color::RESET;
            if (config.controlFlowFlattening) { std::cout << "  Cycles: " << Color::BOLD; config.flatteningCycles = getIntegerInput(); }
            break;
        default: printError("Invalid choice. Defaulting to 'Light'."); config.presetName = "Light"; config.stringObfuscation = true; break;
    }
    printSuccess("Preset configured: " + config.presetName);
    return config;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printHeader("SIH LLVM Obfuscator");
        printError("Usage: ./<executable_name> <initial_source_file.c/.cpp>");
        printInfo("Example", "./build/tools/LLVM_OBFSCALTION.exe tests/hello.c");
        return 1;
    }

    std::string currentInputFile = argv[1];
    ObfuscationConfig currentConfig;
    currentConfig.presetName = "Balanced";
    currentConfig.stringObfuscation = true;
    currentConfig.bogusControlFlow = true;
    currentConfig.fakeLoops = true;
    currentConfig.bogusControlFlowRatio = 30;

    while (true) {
        printHeader("SIH LLVM Obfuscator");
        displayCurrentSettings(currentInputFile, currentConfig);

        std::cout << Color::BOLD << "Main Menu:\n" << Color::RESET;
        std::cout << "  1. Change Input Source File\n  2. Select Obfuscation Preset\n  3. Set Obfuscation Seed (0 for random)\n";
        std::cout << "  4. " << Color::GREEN << "Run Obfuscation Process" << Color::RESET << "\n";
        // --- MODIFICATION: Added new menu items ---
        std::cout << "  5. " << Color::CYAN << "Test Run Obfuscated IR" << Color::RESET << "\n";
        std::cout << "  6. Quit\n";
        
        std::cout << "\nSelect option [1-6]: " << Color::BOLD;
        // --- END MODIFICATION ---
        int choice = getIntegerInput();

        switch (choice) {
            case 1: {
                printStep("Change Input Source File");
                std::cout << "Enter new source file path: " << Color::BOLD;
                std::string newInputFile;
                std::getline(std::cin, newInputFile);
                if (!newInputFile.empty()) currentInputFile = newInputFile;
                std::cout << Color::RESET;
                if (!std::filesystem::exists(currentInputFile)) printError("File not found: " + currentInputFile);
                else printSuccess("Input file set to: " + currentInputFile);
                std::cout << "\nPress Enter to continue..."; std::cin.get();
                break;
            }
            case 2: currentConfig = selectPreset(); std::cout << "\nPress Enter to continue..."; std::cin.get(); break;
            case 3: {
                printStep("Set Obfuscation Seed");
                std::cout << "Enter seed (a number, or 0 for random): " << Color::BOLD;
                currentConfig.seed = getIntegerInput();
                printSuccess("Seed set.");
                std::cout << "\nPress Enter to continue..."; std::cin.get();
                break;
            }
            case 4: {
                std::string outputExeName = "obfuscated_output";
                bool keepFiles = false;
                char yn;

                std::cout << "\nKeep intermediate .ll files? (y/n): " << Color::BOLD;
                std::cin >> yn; keepFiles = (yn == 'y' || yn == 'Y'); std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); std::cout << Color::RESET;
                std::cout << "Enter output executable name (default: '" << outputExeName << "'): " << Color::BOLD;
                std::string customName;
                std::getline(std::cin, customName);
                if (!customName.empty()) outputExeName = customName;
                std::cout << Color::RESET << "\n";

                ObfuscationResult result = performObfuscation(currentInputFile, outputExeName, keepFiles, currentConfig);
                
                if (result.success) {
                    printHeader("Obfuscation Summary");
                    printSuccess("Obfuscation process finished successfully!");
                    std::cout << "\n";
                    printStep("Analysis Comparison");
                    std::cout << Color::BOLD << Color::CYAN << std::left << std::setw(25) << "Metric" << std::setw(15) << "Before" << std::setw(25) << "After" << Color::RESET << "\n";
                    std::cout << "------------------------------------------------------------\n";
                    auto print_analysis_row = [&](const std::string& key, const std::string& display_name) {
                         if (result.initialAnalysis.count(key) && result.finalAnalysis.count(key)) {
                             long long initialVal = result.initialAnalysis[key];
                             long long finalVal = result.finalAnalysis[key];
                             long long change = finalVal - initialVal;
                             std::stringstream afterSS;
                             afterSS << finalVal;
                             if (change != 0) {
                                 double pct_change = (initialVal == 0) ? 100.0 : (double)change / initialVal * 100.0;
                                 afterSS << " (" << (change > 0 ? "+" : "") << change << " | " 
                                         << (change > 0 ? "+" : "") << std::fixed << std::setprecision(1) << pct_change << "%)";
                             }
                              std::cout << Color::CYAN << std::left << std::setw(25) << display_name << Color::RESET 
                                        << std::left << std::setw(15) << initialVal 
                                        << std::left << std::setw(25) << afterSS.str() << "\n";
                         }
                    };
                    print_analysis_row("Instruction Count", "Instruction Count");
                    print_analysis_row("Basic Block Count", "Basic Block Count");
                    print_analysis_row("Code Size (bytes)", "Code Size (bytes)");
                    printStep("Obfuscation Statistics (Changes Made)");
                    if (result.stats.empty()) {
                        std::cout << "  No specific statistics were reported by the passes.\n";
                    } else {
                        for(const auto& pair : result.stats) {
                            printInfo("  " + pair.first, std::to_string(pair.second));
                        }
                    }
                    printStep("Output Files (Absolute Paths)");
                    std::filesystem::path currentPath = std::filesystem::current_path();
                    printInfo("  Executable", (currentPath / outputExeName).string());
                    printInfo("  To Run Executable", "./" + outputExeName);
                    printInfo("  Final Readable LLVM IR", (currentPath / "final_readable_ir.ll").string());
                } else {
                    std::cout << "\n\n" << Color::BOLD << Color::RED;
                    std::cout << "=========================================================\n";
                    std::cout << "                      Obfuscation Failed                 \n";
                    std::cout << "=========================================================\n\n" << Color::RESET;
                    printError("The process encountered an error. Please review the [DEBUG] logs above for details.");
                }
                std::cout << "\nPress Enter to return to the main menu...";
                std::cin.get();
                break;
            }
            // --- MODIFICATION: Added Case 5 for Test Run ---
            case 5: {
                printStep("Test Run Obfuscated IR");
                const std::string irFile = "final_readable_ir.ll";
                if (!std::filesystem::exists(irFile)) {
                    printError("File not found: " + irFile);
                    printInfo("Hint", "Please run the obfuscation process (Option 4) first to generate it.");
                } else {
                    const std::string objFile = "obfuscated_ir.o";
                    const std::string exeFile = "run_obfuscated_ir";
                    const std::string runtimeSrc = "src/runtime/decryptor.c";

                    std::cout << "Compiling IR to object file...\n";
                    if (system(("llc-14 -filetype=obj " + irFile + " -o " + objFile).c_str()) == 0) {
                        printSuccess("IR compiled successfully.");
                        std::cout << "Linking object file with runtime...\n";
                        if (system(("clang-14 " + objFile + " " + runtimeSrc + " -o " + exeFile).c_str()) == 0) {
                            printSuccess("Linking successful. Executing program...");
                            std::cout << "\n" << Color::BOLD << Color::YELLOW << "--- Program Output ---\n" << Color::RESET;
                            system(("./" + exeFile).c_str());
                            std::cout << Color::BOLD << Color::YELLOW << "---  End of Output  ---\n" << Color::RESET;
                        } else {
                            printError("Linking failed. Check compiler output.");
                        }
                    } else {
                        printError("Compiling IR failed. Check llc-14 output.");
                    }

                    // Cleanup
                    std::filesystem::remove(objFile);
                    std::filesystem::remove(exeFile);
                }
                std::cout << "\nPress Enter to continue...";
                std::cin.get();
                break;
            }
            // --- MODIFICATION: Changed Quit to Case 6 ---
            case 6: std::cout << "\nThank you for using the SIH LLVM Obfuscator!\n"; return 0;
            default: printError("Invalid option."); std::cout << "\nPress Enter to continue..."; std::cin.get(); break;
        }
    }

    return 0;
}