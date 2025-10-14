#pragma once

#include "llvm/IR/PassManager.h"
#include <cstdint>

// The DECLARATION of the BogusInsertPass class.
class BogusInsertPass : public llvm::PassInfoMixin<BogusInsertPass> {
private:
    uint32_t Seed_;
    unsigned Inserted_;

public:
    // Constructor declaration
    BogusInsertPass();

    // Run method declaration
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);
};