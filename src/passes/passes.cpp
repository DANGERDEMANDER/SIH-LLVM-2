#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

// Include all your custom pass headers here
#include "StringObfPass.h"
#include "BogusInsertPass.h"
#include "ControlFlowFlatteningPass.h"
#include "FakeLoopPass.h" // <-- ADD THIS INCLUDE

using namespace llvm;

// This is now the ONLY file with llvmGetPassPluginInfo
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, "ObfPasses", "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "string-obf") {
                        MPM.addPass(StringObfPass());
                        return true;
                    }
                    if (Name == "bogus-insert") {
                        MPM.addPass(BogusInsertPass());
                        return true;
                    }
                    if (Name == "cff") {
                        MPM.addPass(createModuleToFunctionPassAdaptor(ControlFlowFlatteningPass()));
                        return true;
                    }
                    // --- ADD THIS BLOCK TO REGISTER THE FAKE LOOP PASS ---
                    if (Name == "fake-loop") {
                        MPM.addPass(createModuleToFunctionPassAdaptor(FakeLoopPass()));
                        return true;
                    }
                    // --- END OF ADDED BLOCK ---
                    return false;
                }
            );
        }
    };
}