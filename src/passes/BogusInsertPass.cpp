#include "BogusInsertPass.h" // Include the declaration

// Add all necessary includes for the implementation here
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <random>
#include <string>

// This is the DEFINITION (implementation) of the class methods.

BogusInsertPass::BogusInsertPass() : Seed_(0x87654321), Inserted_(0) {
    if (const char *env = std::getenv("LLVM_OBF_SEED")) {
        try {
            Seed_ = static_cast<uint32_t>(std::stoul(std::string(env)));
        } catch (...) {
            // Ignore malformed environment value and keep default seed.
        }
    }
}

llvm::PreservedAnalyses
BogusInsertPass::run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
    llvm::LLVMContext &Ctx = M.getContext();
    std::mt19937 rng(Seed_);

    llvm::Type *i32 = llvm::Type::getInt32Ty(Ctx);

    llvm::FunctionType *opaqueFT = llvm::FunctionType::get(i32, {i32}, false);
    llvm::FunctionCallee opaqueFunc =
        M.getOrInsertFunction("__obf_opaque", opaqueFT);

    for (llvm::Function &F : M) {
        if (F.isDeclaration() || F.empty() || F.getName().startswith("__obf_")) {
            continue;
        }

        llvm::BasicBlock *originalEntry = &F.getEntryBlock();

        // --- THE CORRECT FIX: Create the AllocaInst FIRST ---
        // An AllocaInst must be at the top of the function. We create it
        // using the recommended safe IRBuilder constructor before any
        // other modifications are made to the block.
        llvm::IRBuilder<> TopB(originalEntry, originalEntry->begin());
        llvm::AllocaInst *tmp = TopB.CreateAlloca(i32, nullptr, "ob_tmp");

        // Now, find the first instruction to split *after*.
        llvm::Instruction *splitPoint = originalEntry->getFirstNonPHIOrDbgOrLifetime();
        if (!splitPoint || splitPoint->isTerminator()) {
            continue; // Not enough instructions to safely split.
        }

        // We will split the block *after* the splitPoint instruction.
        // This ensures the original block is never empty.
        llvm::BasicBlock *mainPart = originalEntry->splitBasicBlock(splitPoint->getNextNode(), "entry.main");

        // The original entry block now has a terminator jumping to 'mainPart'. Remove it.
        originalEntry->getTerminator()->eraseFromParent();

        // Now, build our bogus logic at the end of the original entry block.
        llvm::IRBuilder<> B(originalEntry);

        uint32_t arg = rng() & 0xFFFF;
        llvm::Value *argV = llvm::ConstantInt::get(i32, arg);
        llvm::CallInst *call = B.CreateCall(opaqueFunc, {argV});
        call->setTailCall(false);

        llvm::Value *masked = B.CreateAnd(call, llvm::ConstantInt::get(i32, 0xFF));
        llvm::Value *cmp = B.CreateICmpEQ(masked, llvm::ConstantInt::get(i32, 0));

        // Create the true/false blocks for our bogus conditional.
        llvm::BasicBlock *bbTrue = llvm::BasicBlock::Create(Ctx, "ob_true", &F, mainPart);
        llvm::BasicBlock *bbFalse = llvm::BasicBlock::Create(Ctx, "ob_false", &F, mainPart);

        // Make the entry block branch to our new blocks
        B.CreateCondBr(cmp, bbTrue, bbFalse);

        // Fill the true block, then branch to the rest of the original function
        llvm::IRBuilder<> TrueB(bbTrue);
        llvm::Value *t1 = TrueB.CreateAdd(llvm::ConstantInt::get(i32, arg),
                                          llvm::ConstantInt::get(i32, 13));
        llvm::Value *t2 = TrueB.CreateMul(t1, llvm::ConstantInt::get(i32, 7));
        TrueB.CreateStore(t2, tmp);
        TrueB.CreateBr(mainPart);

        // Fill the false block, then branch to the rest of the original function
        llvm::IRBuilder<> FalseB(bbFalse);
        llvm::Value *f1 = FalseB.CreateSub(llvm::ConstantInt::get(i32, arg),
                                           llvm::ConstantInt::get(i32, 3));
        llvm::Value *f2 = FalseB.CreateShl(f1, llvm::ConstantInt::get(i32, 2));
        FalseB.CreateStore(f2, tmp);
        FalseB.CreateBr(mainPart);

        ++Inserted_;
    }

    if (Inserted_ > 0) {
        llvm::errs() << "[BogusInsert] inserted " << Inserted_ << " blocks\n";
    }
    return llvm::PreservedAnalyses::all();
}