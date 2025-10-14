#pragma once

#include "llvm/IR/PassManager.h"
#include <cstdint>

// NOTE: The class is now in the global namespace
class StringObfPass : public llvm::PassInfoMixin<StringObfPass> {
private:
    uint32_t Seed;

public:
    StringObfPass();
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};