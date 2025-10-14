// CombinedPassPlugin.cpp
// Single llvmGetPassPluginInfo entrypoint that registers all obfuscation passes.

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

// Forward declarations of registration helpers implemented in each pass file
namespace llvm { class PassBuilder; }
void registerStringObfPass(llvm::PassBuilder &PB);
void registerBogusInsertPass(llvm::PassBuilder &PB);
void registerCFFPass(llvm::PassBuilder &PB);
void registerFakeLoopPass(llvm::PassBuilder &PB);

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ObfPasses-Combined", "v1",
    [](llvm::PassBuilder &PB) {
      // Register all known obfuscation passes so textual names are available
      registerStringObfPass(PB);
      registerBogusInsertPass(PB);
      registerFakeLoopPass(PB);
      registerCFFPass(PB);
    }
  };
}

// C-exported registration helper (callable via dlsym). This allows an
// external process that links LLVM to load the plugin shared object via
// dlopen and call this function to register the passes into a local
// llvm::PassBuilder instance without relying on llvmGetPassPluginInfo.
extern "C" void register_all_obf_passes(void *pb_void) {
  if (!pb_void) return;
  llvm::PassBuilder *PB = static_cast<llvm::PassBuilder*>(pb_void);
  registerStringObfPass(*PB);
  registerBogusInsertPass(*PB);
  registerFakeLoopPass(*PB);
  registerCFFPass(*PB);
}
