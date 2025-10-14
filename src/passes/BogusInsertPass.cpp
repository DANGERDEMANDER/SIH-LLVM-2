// BogusInsertPass.cpp
// Insert opaque calls and small bogus basic-block sequences into functions.

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <string>
#include <random>

namespace {

class BogusInsertPass : public llvm::PassInfoMixin<BogusInsertPass> {
  uint32_t Seed_{0x87654321};
  unsigned Inserted_{0};

public:
  BogusInsertPass() {
    if (const char *env = std::getenv("LLVM_OBF_SEED")) {
      try {
        Seed_ = static_cast<uint32_t>(std::stoul(std::string(env)));
      } catch (...) {
        // Ignore malformed environment value and keep default seed.
      }
    }
  }

  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &)
  {
    llvm::LLVMContext &Ctx = M.getContext();
    std::mt19937 rng(Seed_);

    // opaque function: int __obf_opaque(int)
    llvm::FunctionType *opaqueFT =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(Ctx),
                                {llvm::Type::getInt32Ty(Ctx)}, false);
    llvm::FunctionCallee opaqueFunc = M.getOrInsertFunction("__obf_opaque",
                                                            opaqueFT);

    for (llvm::Function &F : M) {
      if (F.isDeclaration())
        continue;
      if (F.size() < 2)
        continue; // skip tiny functions

      llvm::BasicBlock &entry = F.getEntryBlock();
      llvm::Instruction *insertionPt = &*entry.getFirstInsertionPt();
      llvm::IRBuilder<> B(insertionPt);

      llvm::Type *i32 = llvm::Type::getInt32Ty(Ctx);
      llvm::AllocaInst *tmp = B.CreateAlloca(i32, nullptr, "ob_tmp");

      uint32_t arg = rng() & 0xFFFF;
      llvm::Value *argV = llvm::ConstantInt::get(i32, static_cast<int>(arg));

      llvm::CallInst *call = B.CreateCall(opaqueFunc, {argV});
      call->setTailCall(false);

      // Branch on (call & 0xFF) == 0 and create two small blocks that store
      // different values into the temporary then jump to a continuation.
      llvm::Value *masked =
          B.CreateAnd(call, llvm::ConstantInt::get(i32, 0xFF));
      llvm::Value *cmp =
          B.CreateICmpEQ(masked, llvm::ConstantInt::get(i32, 0));

      llvm::Function *parent = insertionPt->getFunction();

      llvm::BasicBlock *bbTrue =
          llvm::BasicBlock::Create(Ctx, "ob_true", parent);
      llvm::BasicBlock *bbFalse =
          llvm::BasicBlock::Create(Ctx, "ob_false", parent);
      llvm::BasicBlock *cont =
          llvm::BasicBlock::Create(Ctx, "ob_cont", parent);

      B.CreateCondBr(cmp, bbTrue, bbFalse);

      // True block
      llvm::IRBuilder<> TB(bbTrue);
      llvm::Value *t1 = TB.CreateAdd(llvm::ConstantInt::get(i32, arg),
                                      llvm::ConstantInt::get(i32, 13));
      llvm::Value *t2 = TB.CreateMul(t1, llvm::ConstantInt::get(i32, 7));
      TB.CreateStore(t2, tmp);
      TB.CreateBr(cont);

      // False block
      llvm::IRBuilder<> FB(bbFalse);
      llvm::Value *f1 = FB.CreateSub(llvm::ConstantInt::get(i32, arg),
                                      llvm::ConstantInt::get(i32, 3));
      llvm::Value *f2 = FB.CreateShl(f1, llvm::ConstantInt::get(i32, 2));
      FB.CreateStore(f2, tmp);
      FB.CreateBr(cont);

      // Insert continuation and branch back to the original block
      parent->getBasicBlockList().insertAfter(bbFalse->getIterator(), cont);
      llvm::IRBuilder<> CB(cont);
      CB.CreateBr(insertionPt->getParent());

      ++Inserted_;
    }

    llvm::errs() << "[BogusInsert] inserted " << Inserted_ << " inserts\n";
    return llvm::PreservedAnalyses::all();
  }
};

} // end anonymous namespace

// Expose a registration function for programmatic linking (driver)
void registerBogusInsertPass(llvm::PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
    [](llvm::StringRef Name, llvm::ModulePassManager &MPM, llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
      if (Name == "bogus-insert") { MPM.addPass(BogusInsertPass()); return true; }
      return false;
    });
}
