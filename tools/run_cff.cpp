// tools/run_cff.cpp - programmatic runner to load plugin and run 'cff' pipeline
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Error.h"
#include <memory>

using namespace llvm;

static cl::opt<std::string> InputPath(cl::Positional, cl::desc("<input.bc>"), cl::Required);
static cl::opt<std::string> PluginPath("-plugin", cl::desc("Path to plugin"), cl::init("./libObfPasses.so"));
static cl::alias PluginPathShort("-p", cl::aliasopt(PluginPath));
static cl::opt<bool> Verbose("-verbose", cl::desc("Verbose logging"), cl::init(false));
static cl::alias VerboseShort("-v", cl::aliasopt(Verbose));

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "run_cff - programmatic test runner (loads plugin and runs 'cff')\n");

  LLVMContext Ctx;
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseIRFile(InputPath, Err, Ctx);
  if (!M) { Err.print("run_cff", errs()); return 1; }

  // Load plugin explicitly and check for errors
  // Determine plugin path: allow override via RUN_CFF_PLUGIN env var for tests
  std::string PluginToLoad = PluginPath;
  if (const char *envPlugin = std::getenv("RUN_CFF_PLUGIN")) {
    PluginToLoad = std::string(envPlugin);
    if (Verbose) errs() << "[RUN_CFF] overriding plugin path from RUN_CFF_PLUGIN: " << PluginToLoad << "\n";
  } else {
    if (Verbose) errs() << "[RUN_CFF] loading plugin: " << PluginToLoad << "\n";
  }

  auto LoadRes = llvm::PassPlugin::Load(PluginToLoad);
  if (!LoadRes) {
    errs() << "[RUN_CFF] Failed to load plugin: " << llvm::toString(LoadRes.takeError()) << "\n";
    return 1;
  }
  if (Verbose) errs() << "[RUN_CFF] plugin loaded successfully\n";

  PassBuilder PB;

  // Analysis managers
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
  // Parse textual pipeline 'cff'
  if (!PB.parsePassPipeline(MPM, "cff")) {
    errs() << "[RUN_CFF] parsePassPipeline failed for 'cff'\n";
    return 1;
  }

  if (Verbose) errs() << "[RUN_CFF] running pipeline...\n";
  MPM.run(*M, MAM);
  if (Verbose) errs() << "[RUN_CFF] done\n";
  return 0;
}
