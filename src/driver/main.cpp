// src/driver/main.cpp
// Simple CLI driver that programmatically runs passes (LLVM 14).
// This binary links the pass implementation files directly (not as a plugin)
// so `llvm_obfuscation` is a single executable.

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

static cl::opt<std::string> InputIR("ir", cl::desc("Input LLVM bitcode or IR file (.bc/.ll)"), cl::init(""));
static cl::opt<std::string> InputSrc("src", cl::desc("Input C/C++ source file to compile first (optional)"), cl::init(""));
static cl::opt<std::string> OutputBin("out", cl::desc("Output native binary"), cl::init("dist/main_obf"));
static cl::opt<std::string> Preset("preset", cl::desc("Obfuscation preset: light|balanced|aggressive"), cl::init("balanced"));
static cl::opt<int> Seed("seed", cl::desc("Optional seed (0 = random)"), cl::init(0));
static cl::opt<bool> KeepBC("keep-bc", cl::desc("Keep intermediate bitcode files"), cl::init(false));

extern void registerStringObfPass(PassBuilder &PB);
extern void registerBogusInsertPass(PassBuilder &PB);
extern void registerCFFPass(PassBuilder &PB);

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "LLVM obfuscation CLI\n");

  // Initialize targets for llc/linking later
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  std::string work_ir = InputIR;
  std::filesystem::path autoBC;
  bool createdTemporaryBC = false;
  if (work_ir.empty()) {
    if (InputSrc.empty()) {
      errs() << "Either --ir <file.bc> or --src <file.c> must be provided\n";
      return 1;
    }

    // choose clang binary (prefer clang-14)
    std::string clangBin = "clang-14";
    if (system((clangBin + " --version > /dev/null 2>&1").c_str()) != 0)
      clangBin = "clang";

    // ensure build directory exists
    std::filesystem::create_directories("build");

    // create a unique temporary output path
    auto stamp = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    autoBC = std::filesystem::path("build") / (std::string("auto_generated_") + stamp + ".bc");

    // determine language
    std::string langArg = "c++";
    if (InputSrc.size() >= 2 && InputSrc.substr(InputSrc.size()-2) == ".c") langArg = "c";
    std::string cmd = clangBin + " -x " + langArg + " -emit-llvm -c -g -O0 " + InputSrc + " -o " + autoBC.string();
    errs() << "[driver] Running: " << cmd << "\n";
    if (system(cmd.c_str()) != 0) {
      errs() << "clang compile failed\n";
      return 1;
    }
    work_ir = autoBC.string();
    createdTemporaryBC = true;
  }

  LLVMContext ctx;
  SMDiagnostic err;
  std::unique_ptr<Module> M = parseIRFile(work_ir, err, ctx);
  if (!M) {
    errs() << "Failed to parse IR: " << work_ir << "\n";
    err.print(argv[0], errs());
    return 1;
  }

  // Set up PassBuilder and analysis managers
  PassBuilder PB;
  ModuleAnalysisManager MAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  LoopAnalysisManager LAM;

  // register analyses with PB
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  // --- Register our passes into PassBuilder so we can use textual pipeline or programmatically add them
  registerStringObfPass(PB);
  registerBogusInsertPass(PB);
  registerCFFPass(PB);

  // Create a ModulePassManager and add passes based on preset
  ModulePassManager MPM;
  // Use textual pipeline strings to select passes; registration functions above make
  // the elements available to PassBuilder even when linked statically.
  std::string pipeline;
  if (Preset == "light") {
    pipeline = "module(string-obf)";
  } else if (Preset == "balanced") {
    pipeline = "module(string-obf,bogus-insert)";
  } else {
    pipeline = "module(string-obf,bogus-insert,control-flow-flattening)";
  }
  if (!PB.parsePassPipeline(MPM, pipeline)) {
    errs() << "[driver] parsePassPipeline failed for: " << pipeline << "\n";
    return 1;
  } else {
    errs() << "[driver] Using pipeline: " << pipeline << "\n";
  }

  // If the user supplied a seed, export it to environment so passes can pick it up
  if (Seed != 0) {
    std::string seedEnv = "LLVM_OBF_SEED=" + std::to_string(Seed);
    putenv(const_cast<char*>(seedEnv.c_str()));
  }

  // Run MPM
  MPM.run(*M, MAM);

  // write obfuscated bitcode
  std::error_code ec;
  std::string outbc = "build/main_obf.bc";
  std::filesystem::create_directories(std::filesystem::path(outbc).parent_path());
  raw_fd_ostream ofs(outbc, ec, sys::fs::OF_None);
  if (ec) {
    errs() << "Failed to open " << outbc << ": " << ec.message() << "\n";
    return 1;
  }
  WriteBitcodeToFile(*M, ofs);
  ofs.flush();

  // Lower to object and link runtime to produce native binary
  // llc + clang steps
  std::string obj = "build/main_obf.o";
  std::string cmd_llc = "llc -filetype=obj " + outbc + " -o " + obj;
  errs() << "[driver] Running: " << cmd_llc << "\n";
  if (system(cmd_llc.c_str()) != 0) return 1;
  // link with runtime (use detected clang above if available)
  std::string clangLink = "clang-14";
  if (system((clangLink + " --version > /dev/null 2>&1").c_str()) != 0) clangLink = "clang";
  std::string cmd_link = clangLink + " " + obj + " src/runtime/decryptor.c -o " + OutputBin;
  errs() << "[driver] Running: " << cmd_link << "\n";
  if (system(cmd_link.c_str()) != 0) return 1;

  if (!KeepBC) {
    // remove intermediate files we created
    try {
      if (createdTemporaryBC && !work_ir.empty()) std::filesystem::remove(work_ir);
      std::filesystem::remove(outbc);
      std::filesystem::remove(obj);
    } catch (...) {}
  }

  outs() << "[driver] Built: " << OutputBin << "\n";
  return 0;
}
