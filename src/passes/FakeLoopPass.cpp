#include "FakeLoopPass.h" // Use the new header

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <random>

using namespace llvm;

// Constructor implementation
FakeLoopPass::FakeLoopPass() : Seed_(0xfeedbeef), Inserted_(0) {
    if (const char *env = std::getenv("LLVM_OBF_SEED")) {
        try { Seed_ = static_cast<uint32_t>(std::stoul(std::string(env))); } catch(...) {}
    }
}

// Run method implementation
PreservedAnalyses FakeLoopPass::run(Function &F, FunctionAnalysisManager &AM) {
    if (F.isDeclaration() || F.empty() || F.getName().startswith("__obf_")) {
        return PreservedAnalyses::all();
    }

    LLVMContext &Ctx = F.getContext();
    std::mt19937 rng(Seed_);
    
    BasicBlock *entryBlock = &F.getEntryBlock();
    
    // Find the first valid instruction to split from.
    BasicBlock::iterator firstRealInst = entryBlock->getFirstInsertionPt();

    if (firstRealInst == entryBlock->end() || entryBlock->isEHPad()) {
        return PreservedAnalyses::all();
    }
    
    // 1. Create the new blocks for the loop structure.
    BasicBlock *afterLoop = BasicBlock::Create(Ctx, "fake.loop.after", &F, entryBlock->getNextNode());
    BasicBlock *loopBody = BasicBlock::Create(Ctx, "fake.loop.body", &F, afterLoop);
    BasicBlock *loopHeader = BasicBlock::Create(Ctx, "fake.loop.header", &F, loopBody);

    // 2. Move all instructions from the first real one to the end of the entry block
    //    into our 'afterLoop' block. This clears the way for the loop.
    afterLoop->getInstList().splice(afterLoop->begin(), entryBlock->getInstList(), firstRealInst, entryBlock->end());
    
    // 3. Make the original entry block jump to the loop header.
    entryBlock->getTerminator()->eraseFromParent();
    IRBuilder<>(entryBlock).CreateBr(loopHeader);

    // 4. Populate the loop header (the part that runs once).
    IRBuilder<> headerBuilder(loopHeader);
    Type *I32 = Type::getInt32Ty(Ctx);
    AllocaInst *cnt = headerBuilder.CreateAlloca(I32, nullptr, "fake_cnt");
    headerBuilder.CreateStore(ConstantInt::get(I32, (rng() % 5) + 3), cnt); // Loop 3-7 times
    headerBuilder.CreateBr(loopBody);

    // 5. Populate the loop body (the part that repeats).
    IRBuilder<> bodyBuilder(loopBody);
    LoadInst *v = bodyBuilder.CreateLoad(I32, cnt, "fake_val");
    Value *dec = bodyBuilder.CreateSub(v, ConstantInt::get(I32, 1), "fake_dec");
    bodyBuilder.CreateStore(dec, cnt);

    Value *cond = bodyBuilder.CreateICmpSGT(dec, ConstantInt::get(I32, 0), "fake_cond");
    bodyBuilder.CreateCondBr(cond, loopBody, afterLoop); // If condition is true, loop again; otherwise, exit to afterLoop.

    ++Inserted_;
    errs() << "[FakeLoop] inserted " << Inserted_ << " loops\n";
    
    return PreservedAnalyses::none();
}