#pragma once

#include "llvm/IR/PassManager.h"
#include <cstdint>

// Declaration of the FakeLoopPass class
class FakeLoopPass : public llvm::PassInfoMixin<FakeLoopPass> {
private:
    uint32_t Seed_;
    unsigned Inserted_;

public:
    // Constructor
    FakeLoopPass();

    // The main run method for the pass
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
};