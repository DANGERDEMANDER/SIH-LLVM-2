// StringObfPass.cpp - encrypt global strings and replace uses with runtime decryptor
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <string>
#include <vector>
using namespace llvm;
namespace {
static uint32_t next_key(uint32_t &seed) {
  uint32_t x = seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  seed = x;
  return x ? x : 0xdeadbeef;
}
class StringObfPass : public PassInfoMixin<StringObfPass> {
  uint32_t Seed;
  unsigned CountEncrypted;
  uint64_t TotalBytes;
public:
  StringObfPass() : Seed(0x12345678), CountEncrypted(0), TotalBytes(0) {
    if (const char *env = std::getenv("LLVM_OBF_SEED")) Seed = (uint32_t)std::stoul(env);
  }
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    LLVMContext &Ctx = M.getContext();
    FunctionCallee decryptor = M.getOrInsertFunction("__obf_decrypt",
      FunctionType::get(Type::getInt8PtrTy(Ctx),
                        {Type::getInt8PtrTy(Ctx), Type::getInt32Ty(Ctx), Type::getInt32Ty(Ctx)}, false));
    std::vector<GlobalVariable*> globalsToProcess;
    for (GlobalVariable &GV : M.globals()) globalsToProcess.push_back(&GV);
    for (GlobalVariable *GV : globalsToProcess) {
      if (!GV->hasInitializer()) continue;
      if (auto *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer())) {
        if (!CDA->isString()) continue;
        StringRef s = CDA->getAsString(); if (s.empty()) continue;
        uint32_t key = next_key(Seed); std::string enc; enc.resize(s.size());
        unsigned char kb = static_cast<unsigned char>(key & 0xFF);
        for (size_t i = 0; i < s.size(); ++i) enc[i] = static_cast<char>(s[i] ^ kb);
        ArrayType *arrTy = ArrayType::get(Type::getInt8Ty(Ctx), enc.size());
        Constant *encInit = ConstantDataArray::getString(Ctx, StringRef(enc), false);
        GlobalVariable *encGV = new GlobalVariable(M, arrTy, true, GlobalValue::PrivateLinkage, encInit, GV->getName() + ".enc");
        encGV->setAlignment(MaybeAlign(1));
        std::vector<User*> uses(GV->user_begin(), GV->user_end());
        for (User *U : uses) if (Instruction *I = dyn_cast<Instruction>(U)) {
          IRBuilder<> B(I);
          Value *zero = ConstantInt::get(Type::getInt32Ty(Ctx), 0);
          Value *gep = B.CreateInBoundsGEP(encGV->getValueType(), encGV, {zero, zero});
          Value *lenVal = ConstantInt::get(Type::getInt32Ty(Ctx), (uint32_t)enc.size());
          Value *keyVal = ConstantInt::get(Type::getInt32Ty(Ctx), (uint32_t)key);
          CallInst *call = B.CreateCall(decryptor, {gep, lenVal, keyVal});
          Type *origTy = U->getType();
          if (origTy != call->getType()) {
            if (origTy->isPointerTy() && call->getType()->isPointerTy()) {
              call = cast<CallInst>(CastInst::CreatePointerCast(call, origTy, "cast.str", I));
              I->getParent()->getInstList().insert(I->getIterator(), cast<Instruction>(call));
            }
          }
          I->replaceUsesOfWith(GV, call);
        }
        Constant *zeroInit = ConstantAggregateZero::get(encGV->getValueType());
        GV->setInitializer(zeroInit);
        ++CountEncrypted; TotalBytes += enc.size();
      }
    }
    std::string json = "{\\n  \\\"num_strings_encrypted\\\": " + std::to_string(CountEncrypted) + ",\\n  \\\"total_string_bytes\\\": " + std::to_string(TotalBytes) + "\\n}\\n";
    errs() << "[StringObf] encrypted = " << CountEncrypted << " bytes = " << TotalBytes << "\\n";
    if (const char* of = std::getenv("OFILE")) { std::error_code ec; raw_fd_ostream os(of, ec); if (!ec) os << json; } else { outs() << json; }
    return PreservedAnalyses::all();
  }
};
}
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "string-obf-pass", "v1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "string-obf") { MPM.addPass(StringObfPass()); return true; }
          return false;
        });
    }};
}
