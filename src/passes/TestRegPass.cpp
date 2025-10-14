// TestRegPass.cpp - minimal pass to verify PassPlugin registration
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct TestRegPass : public PassInfoMixin<TestRegPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    errs() << "[TESTREG] running on function: " << F.getName() << "\n";
    return PreservedAnalyses::all();
  }
};
} // anonymous

static void test_plugin_ctor() __attribute__((constructor));
static void test_plugin_ctor() { errs() << "[TESTREG_PLUGIN] ctor called\n"; }

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "testreg-plugin", "v0.1",
          [](PassBuilder &PB) {
            errs() << "[TESTREG] registering pipeline callbacks\n";
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  errs() << "[TESTREG] function-callback: Name='" << Name << "'\n";
                  if (Name == "testreg") {
                    FPM.addPass(TestRegPass());
                    errs() << "[TESTREG] function-callback: registered TestRegPass\n";
                    return true;
                  }
                  return false;
                });

            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  errs() << "[TESTREG] module-callback: Name='" << Name << "'\n";
                  if (Name == "testreg") {
                    MPM.addPass(createModuleToFunctionPassAdaptor(TestRegPass()));
                    errs() << "[TESTREG] module-callback: registered module adapter\n";
                    return true;
                  }
                  return false;
                });
          }};
}
