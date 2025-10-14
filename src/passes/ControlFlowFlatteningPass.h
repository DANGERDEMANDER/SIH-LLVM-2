#pragma once

#include "llvm/IR/PassManager.h"

// NOTE: The class is now in the global namespace
class ControlFlowFlatteningPass : public llvm::PassInfoMixin<ControlFlowFlatteningPass> {
public:
    llvm::PreservedAnalyses run(llvm::Function &F, llvm::FunctionAnalysisManager &AM);
};