// FakeLoopPass.cpp
// Insert simple fake loops to increase code complexity and confuse static analysis.

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <random>

using namespace llvm;

namespace {

class FakeLoopPass : public PassInfoMixin<FakeLoopPass> {
  uint32_t Seed_{0xfeedbeef};
  unsigned Inserted_ = 0;

public:
  FakeLoopPass() {
    if (const char *env = std::getenv("LLVM_OBF_SEED")) {
      try { Seed_ = static_cast<uint32_t>(std::stoul(std::string(env))); } catch(...) {}
    }
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    if (F.isDeclaration() || F.size() < 3) return PreservedAnalyses::all();
    LLVMContext &Ctx = F.getContext();
    std::mt19937 rng(Seed_);

    // Insert a tiny loop in the entry block that runs a couple of cheap ops
    IRBuilder<> B(&*F.getEntryBlock().getFirstInsertionPt());
    Type *I32 = Type::getInt32Ty(Ctx);
    AllocaInst *cnt = B.CreateAlloca(I32, nullptr, "fake_cnt");
    B.CreateStore(ConstantInt::get(I32, 3), cnt);

    BasicBlock *loopBB = BasicBlock::Create(Ctx, "fake.loop", &F);
    BasicBlock *afterBB = BasicBlock::Create(Ctx, "fake.after", &F);

    B.CreateBr(loopBB);
    // loop body
    IRBuilder<> LB(loopBB);
    LoadInst *v = LB.CreateLoad(I32, cnt);
    Value *dec = LB.CreateSub(v, ConstantInt::get(I32, 1));
    LB.CreateStore(dec, cnt);
    // cheap opaque arithmetic
    Value *tmp = LB.CreateAdd(dec, ConstantInt::get(I32, 7));
    (void)LB.CreateLShr(tmp, ConstantInt::get(I32, 1));
    Value *cond = LB.CreateICmpSGT(dec, ConstantInt::get(I32, 0));
    LB.CreateCondBr(cond, loopBB, afterBB);

    // Continue
    IRBuilder<> AB(afterBB);
    AB.CreateBr(&F.getEntryBlock());

    ++Inserted_;
    errs() << "[FakeLoop] inserted " << Inserted_ << " loops\n";
    return PreservedAnalyses::none();
  }
};

} // namespace

// registration for programmatic linking
void registerFakeLoopPass(llvm::PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
    [](llvm::StringRef Name, llvm::ModulePassManager &MPM, llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
      if (Name == "fake-loop") { MPM.addPass(llvm::createModuleToFunctionPassAdaptor(FakeLoopPass())); return true; }
      return false;
    });
}
