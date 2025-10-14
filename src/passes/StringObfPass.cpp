#include "StringObfPass.h" // Use the header for the declaration

#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <vector>

// Note: The 'using namespace' is fine, but the 'namespace llvm { ... }' wrapper is removed.
using namespace llvm;

// Implementation of the constructor from your original code
StringObfPass::StringObfPass() : Seed(0x12345678) {
  if (const char *env = std::getenv("LLVM_OBF_SEED")) {
    Seed = (uint32_t)std::stoul(env);
  }
}

// Implementation of the run method from your original code
PreservedAnalyses StringObfPass::run(Module &M, ModuleAnalysisManager &AM) {
  uint32_t current_seed = Seed;
  unsigned CountEncrypted = 0;
  uint64_t TotalBytes = 0;

  auto next_key = [&]() -> uint32_t {
    uint32_t x = current_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    current_seed = x;
    return x ? x : 0xdeadbeef;
  };

  LLVMContext &Ctx = M.getContext();
  FunctionCallee decryptor = M.getOrInsertFunction(
      "__obf_decrypt",
      FunctionType::get(Type::getInt8PtrTy(Ctx),
                        {Type::getInt8PtrTy(Ctx), Type::getInt32Ty(Ctx),
                         Type::getInt32Ty(Ctx)},
                        false));

  std::vector<GlobalVariable *> globalsToProcess;
  for (GlobalVariable &GV : M.globals()) {
    globalsToProcess.push_back(&GV);
  }

  for (GlobalVariable *GV : globalsToProcess) {
    if (!GV->hasInitializer() || !GV->isConstant() ||
        !GV->hasPrivateLinkage())
      continue;

    if (auto *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer())) {
      if (!CDA->isString())
        continue;
      StringRef s = CDA->getAsString();
      if (s.size() <= 1)
        continue;

      uint32_t key = next_key();
      std::string enc;
      enc.resize(s.size() - 1);
      unsigned char kb = static_cast<unsigned char>(key & 0xFF);
      for (size_t i = 0; i < s.size() - 1; ++i) {
        enc[i] = static_cast<char>(s[i] ^ kb);
      }

      Constant *encInit = ConstantDataArray::getString(Ctx, enc, false);
      GlobalVariable *encGV = new GlobalVariable(
          M, encInit->getType(), true, GlobalValue::PrivateLinkage, encInit,
          GV->getName() + ".enc");
      encGV->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

      std::vector<User *> uses;
      for (auto u : GV->users()) {
        uses.push_back(u);
      }

      for (User *U : uses) {
        if (auto *I = dyn_cast<Instruction>(U)) {
          IRBuilder<> B(I);
          Value *gep = B.CreateInBoundsGEP(
              encGV->getValueType(), encGV,
              {ConstantInt::get(Type::getInt32Ty(Ctx), 0),
               ConstantInt::get(Type::getInt32Ty(Ctx), 0)});
          Value *lenVal =
              ConstantInt::get(Type::getInt32Ty(Ctx), (uint32_t)enc.size());
          Value *keyVal = ConstantInt::get(Type::getInt32Ty(Ctx), (uint32_t)key);
          CallInst *call = B.CreateCall(decryptor, {gep, lenVal, keyVal});
          I->replaceUsesOfWith(GV, call);
        }
      }

      if (GV->use_empty()) {
        GV->eraseFromParent();
      }

      ++CountEncrypted;
      TotalBytes += enc.size();
    }
  }

  if (const char *of = std::getenv("OFILE")) {
    std::error_code EC;
    raw_fd_ostream os(of, EC);
    if (!EC) {
      os << "{\n";
      os << "  \"num_strings_encrypted\": " << CountEncrypted << ",\n";
      os << "  \"total_string_bytes\": " << TotalBytes << "\n";
      os << "}\n";
    }
  }
  return PreservedAnalyses::all();
}