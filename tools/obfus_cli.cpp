// Programmatic obfuscator runner
// Loads a PassPlugin, parses a textual pipeline, runs it on a module, and
// writes the resulting bitcode.

#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Program.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace llvm;

static cl::opt<std::string> InputPath(cl::Positional, cl::desc("<input.bc | input.c | input.cpp | - (stdin)>") );
static cl::opt<std::string> OutputPath("-o", cl::desc("Output file"), cl::init("out.bc"));
static cl::opt<std::string> PluginPath("-plugin", cl::desc("Path to plugin"), cl::init("./libObfPasses.so"));
static cl::opt<std::string> PassName("-pass", cl::desc("Pass pipeline element name (e.g. cff)"), cl::init("cff"));
static cl::opt<bool> Verbose("-v", cl::desc("Verbose output"), cl::init(false));
static cl::opt<bool> Interactive("-i", cl::desc("Interactive mode: prompt for source or paste code"), cl::init(false));
static cl::opt<std::string> Lang("-lang", cl::desc("Language hint for source inputs: c or c++ (default: autodetect)"), cl::init(""));

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "obfuscator - programmatic obfuscation runner\n");

  LLVMContext Ctx;
  SMDiagnostic Err;

  // Determine input source: bitcode/IR or source code. Support interactive mode.
  std::string UserInput = InputPath;
  std::filesystem::path TempSrcPath;
  std::filesystem::path TempBcPath;
  bool CreatedTempFiles = false;

  if (Interactive) {
    outs() << "Interactive obfuscator\n";
    outs() << "Enter path to C/C++ source file, or single '-' to paste code (end with EOF):\n> ";
    std::string line;
    if (!std::getline(std::cin, line)) {
      errs() << "No input provided\n";
      return 1;
    }
    if (line == "-") {
      // Read remainder of stdin until EOF
      std::ostringstream ss;
      while (std::getline(std::cin, line)) ss << line << "\n";
      // Default to C++ unless user specified -lang=c
      std::string ext = (Lang == "c") ? ".c" : ".cpp";
      auto tmpdir = std::filesystem::temp_directory_path();
      auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
      TempSrcPath = tmpdir / (std::string("obf_src_") + stamp + ext);
      std::ofstream ofs(TempSrcPath);
      ofs << ss.str();
      ofs.close();
      UserInput = TempSrcPath.string();
      CreatedTempFiles = true;
    } else {
      UserInput = line;
    }
  }

  // If input is '-' treat as stdin (attempt to read piped IR)
  if (UserInput == "-") {
    // Read stdin into a temporary file (assume IR/bitcode or source)
    auto tmpdir = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    TempSrcPath = tmpdir / (std::string("obf_stdin_") + stamp + ".in");
    std::ofstream ofs(TempSrcPath);
    std::string s;
    while (std::getline(std::cin, s)) ofs << s << "\n";
    ofs.close();
    UserInput = TempSrcPath.string();
    CreatedTempFiles = true;
  }

  auto tryParseIR = [&](const std::string &Path)->std::unique_ptr<Module> {
    SMDiagnostic e;
    LLVMContext &c = Ctx;
    return parseIRFile(Path, e, c);
  };

  std::unique_ptr<Module> M;

  // Decide whether the input is a bitcode/IR file or a C/C++ source file
  std::filesystem::path P(UserInput);
  std::string Ext = P.extension().string();
  if (Ext == ".bc" || Ext == ".ll" ) {
    M = tryParseIR(UserInput);
    if (!M) { Err.print("obfuscator", errs()); return 1; }
  } else {
    // Treat as source: invoke clang to compile to bitcode
    auto tmpdir = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    TempBcPath = tmpdir / (std::string("obf_tmp_") + stamp + ".bc");
    std::string OutBc = TempBcPath.string();

    // Determine language
    std::string langArg;
    if (Lang == "c") langArg = "c";
    else if (Lang == "c++") langArg = "c++";
    else if (Ext == ".c") langArg = "c";
    else langArg = "c++";

    std::vector<std::string> CmdArgs;
    CmdArgs.push_back("clang");
    CmdArgs.push_back("-x");
    CmdArgs.push_back(langArg);
    CmdArgs.push_back("-O0");
    CmdArgs.push_back("-emit-llvm");
    CmdArgs.push_back("-c");
    CmdArgs.push_back(UserInput);
    CmdArgs.push_back("-o");
    CmdArgs.push_back(OutBc);

    if (Verbose) {
      errs() << "[OBF] compiling source to bitcode with clang: ";
      for (auto &s : CmdArgs) errs() << s << " ";
      errs() << "\n";
    }

    // Convert to StringRef vector for ExecuteAndWait
    std::vector<StringRef> ArgRefs;
    for (auto &s : CmdArgs) ArgRefs.push_back(StringRef(s));

    int Ret = sys::ExecuteAndWait(ArgRefs[0], ArgRefs);
    if (Ret != 0) {
      errs() << "[OBF] clang failed (exit=" << Ret << ") compiling '" << UserInput << "'\n";
      return 4;
    }

    // Load the produced bitcode
    M = tryParseIR(OutBc);
    if (!M) { Err.print("obfuscator", errs()); return 1; }
    CreatedTempFiles = true;
    // Make InputPath point to produced BC for later reference
    InputPath = OutBc;
  }

  std::string PluginToLoad = PluginPath;
  if (const char *envPlugin = std::getenv("OBF_PLUGIN")) {
    PluginToLoad = std::string(envPlugin);
    if (Verbose) errs() << "[OBF] overriding plugin path from OBF_PLUGIN: " << PluginToLoad << "\n";
  }

  if (Verbose) errs() << "[OBF] loading plugin: " << PluginToLoad << "\n";
  auto LoadRes = PassPlugin::Load(PluginToLoad);
  if (!LoadRes) {
    errs() << "[OBF] Failed to load plugin: " << toString(LoadRes.takeError()) << "\n";
    return 1;
  }
  if (Verbose) errs() << "[OBF] plugin loaded successfully\n";

  PassBuilder PB;
  ModuleAnalysisManager MAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  LoopAnalysisManager LAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  ModulePassManager MPM;
  if (!PB.parsePassPipeline(MPM, PassName)) {
    errs() << "[OBF] parsePassPipeline failed for '" << PassName << "'\n";
    return 2;
  }

  if (Verbose) errs() << "[OBF] running pipeline...\n";
  MPM.run(*M, MAM);
  if (Verbose) errs() << "[OBF] pipeline complete, writing output to: " << OutputPath << "\n";

  std::error_code EC;
  raw_fd_ostream Out(OutputPath, EC);
  if (EC) {
    errs() << "[OBF] Failed to open output file '" << OutputPath << "': " << EC.message() << "\n";
    return 3;
  }
  WriteBitcodeToFile(*M, Out);
  Out.flush();
  if (Verbose) errs() << "[OBF] wrote output successfully\n";
  return 0;
}
