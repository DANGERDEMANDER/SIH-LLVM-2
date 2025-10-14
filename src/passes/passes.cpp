#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

// Include your custom pass headers here
#include "StringObfPass.h"
#include "BogusInsertPass.h"
#include "ControlFlowFlatteningPass.h"

using namespace llvm;

// ... rest of your code
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "ObfPasses", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "string-obf") {
                        MPM.addPass(StringObfPass()); // This will now work
                        return true;
                    }
                    if (Name == "bogus-insert") {
                        MPM.addPass(BogusInsertPass()); // This will now work
                        return true;
                    }
                    if (Name == "cff") {
                        MPM.addPass(createModuleToFunctionPassAdaptor(ControlFlowFlatteningPass())); // This will now work
                        return true;
                    }
                    return false;
                }
            );
        }
    };
}