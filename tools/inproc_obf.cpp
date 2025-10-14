// tools/inproc_obf.cpp
// In-process obfuscator: loads plugin via dlopen, calls registration helper,
// and runs a textual pipeline using PassBuilder. Useful as a fallback when
// opt cannot load textual pass names.

#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"

#include <dlfcn.h>
#include <memory>
#include <string>

using namespace llvm;

static cl::opt<std::string> InputPath(cl::Positional, cl::desc("<input.bc>"), cl::Required);
static cl::opt<std::string> PluginPath("-plugin", cl::desc("Path to plugin"), cl::init("./libObfPasses.so"));
static cl::opt<std::string> PassName("-passes", cl::desc("Textual pipeline (e.g. string-obf,bogus-insert)"), cl::init("string-obf"));
static cl::opt<std::string> OutputPath("-o", cl::desc("Output bitcode"), cl::init("out_obf.bc"));

using register_fn_t = void(*)(void*);

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv, "inproc_obf - in-process obfuscation runner\n");

  LLVMContext Ctx;
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseIRFile(InputPath, Err, Ctx);
  if (!M) { Err.print("inproc_obf", errs()); return 1; }

  void *hdl = dlopen(PluginPath.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!hdl) { errs() << "dlopen failed: " << dlerror() << "\n"; return 2; }

  auto sym = (void*)dlsym(hdl, "register_all_obf_passes");
  if (!sym) { errs() << "dlsym(register_all_obf_passes) failed: " << dlerror() << "\n"; dlclose(hdl); return 3; }

  PassBuilder PB;
  // Call register helper to populate PB with pass registrations
  register_fn_t reg = reinterpret_cast<register_fn_t>(sym);
  reg(static_cast<void*>(&PB));

  ModuleAnalysisManager MAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  LoopAnalysisManager LAM;

  PB.registerModuleAnalyses(MAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  ModulePassManager MPM;
  if (!PB.parsePassPipeline(MPM, PassName)) {
    errs() << "parsePassPipeline failed for '" << PassName << "'\n";
    dlclose(hdl);
    return 4;
  }

  MPM.run(*M, MAM);

  std::error_code EC;
  raw_fd_ostream Out(OutputPath, EC);
  if (EC) { errs() << "Failed to open output: " << EC.message() << "\n"; dlclose(hdl); return 5; }
  WriteBitcodeToFile(*M, Out);
  Out.flush();

  dlclose(hdl);
  return 0;
}
