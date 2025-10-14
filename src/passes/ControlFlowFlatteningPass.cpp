// ControlFlowFlatteningPass.cpp
// Converts function control flow into a switch-based dispatch loop.
// Security: Disrupts static analysis by making all blocks appear to be possible
// successors of each other, while preserving original semantics.

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <random>
#include <unordered_map>

using namespace llvm;

// Library-load debug: prints when the plugin is dlopen'd. This helps verify
// whether opt actually loads the shared object and runs static init.
static void plugin_load_debug() __attribute__((constructor));
static void plugin_load_debug() {
  errs() << "[CFF_PLUGIN] plugin constructor called\n";
}

namespace {

class ControlFlowFlatteningPass : public PassInfoMixin<ControlFlowFlatteningPass> {
  uint32_t Seed_{0x12345678};
  unsigned NumFlattened_{0};

public:
  ControlFlowFlatteningPass() {
    if (const char *env = std::getenv("LLVM_OBF_SEED"))
      try {
        Seed_ = static_cast<uint32_t>(std::stoul(std::string(env)));
      } catch (...) {}
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    if (F.isDeclaration() || F.size() < 3)
      return PreservedAnalyses::all();

    LLVMContext &Ctx = F.getContext();
    std::mt19937 RNG(Seed_);

    // Create state variables at function entry
  IRBuilder<> Builder(&*F.begin());
  IntegerType *I32Ty = Type::getInt32Ty(Ctx);
    AllocaInst *StateVar = Builder.CreateAlloca(I32Ty, nullptr, "cff.state");
    
    // Map each original block to a unique state value
    std::unordered_map<BasicBlock*, uint32_t> BlockToState;
    uint32_t NextState = 1;
    for (BasicBlock &BB : F) {
      if (&BB == &F.getEntryBlock())
        BlockToState[&BB] = 0;
      else
        BlockToState[&BB] = NextState++;
    }

    // Create dispatch block that will contain the switch
    BasicBlock *DispatchBB = BasicBlock::Create(
        Ctx, "cff.dispatch", &F, &F.getEntryBlock());
    
    // Move all blocks except entry after dispatch
    for (auto &BB : F) {
      if (&BB != &F.getEntryBlock() && &BB != DispatchBB) {
        BB.moveAfter(DispatchBB);
      }
    }

    // Create switch in dispatch block
    Builder.SetInsertPoint(DispatchBB);
    LoadInst *CurrentState = Builder.CreateLoad(I32Ty, StateVar, "cff.current");
    SwitchInst *Dispatcher = Builder.CreateSwitch(
        CurrentState, &F.getEntryBlock(), BlockToState.size());

    // Process each block: redirect terminators to dispatch block
    for (auto &BB : F) {
      if (&BB == DispatchBB)
        continue;

  // Add this block as a switch case
  ConstantInt *CaseVal = ConstantInt::get(I32Ty, BlockToState[&BB]);
      Dispatcher->addCase(CaseVal, &BB);

      // Get terminator
      Instruction *Term = BB.getTerminator();
      if (BranchInst *BI = dyn_cast<BranchInst>(Term)) {
        Builder.SetInsertPoint(Term);

        if (BI->isConditional()) {
          // Convert conditional branch to state update + jump to dispatch
          Value *Cond = BI->getCondition();
          Value *TrueState = ConstantInt::get(
              I32Ty, BlockToState[BI->getSuccessor(0)]);
          Value *FalseState = ConstantInt::get(
              I32Ty, BlockToState[BI->getSuccessor(1)]);
          Value *NextState = Builder.CreateSelect(
              Cond, TrueState, FalseState, "cff.next");
          Builder.CreateStore(NextState, StateVar);
          Builder.CreateBr(DispatchBB);
        } else if (BI->isUnconditional() && 
                   BI->getSuccessor(0) != DispatchBB) {
          // Convert unconditional branch to state update + jump to dispatch
          Value *NextState = ConstantInt::get(
              I32Ty, BlockToState[BI->getSuccessor(0)]);
          Builder.CreateStore(NextState, StateVar);
          Builder.CreateBr(DispatchBB);
        }
        
        // Remove old terminator
        Term->eraseFromParent();
      }
    }

    // Initialize state to 0 (entry block) in the entry block
    Builder.SetInsertPoint(&F.getEntryBlock().front());
    Builder.CreateStore(ConstantInt::get(I32Ty, 0), StateVar);

    ++NumFlattened_;
    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "control-flow-flattening", "v0.1",
          [](PassBuilder &PB) {
            errs() << "[CFF] registering callbacks\n";
            // Register a single module-level pipeline element 'cff'. This is
            // the canonical pattern used by LLVM's new PM examples: when the
            // parser sees -passes="cff" it will invoke this callback and we
            // adapt the function pass into the module pipeline.
            PB.registerPipelineParsingCallback(
              [](StringRef Name, ModulePassManager &MPM,
                 ArrayRef<PassBuilder::PipelineElement> Elements) {
                errs() << "[CFF] module-callback: received Name='" << Name << "'\n";
                if (Name == "cff") {
                  errs() << "[CFF] module-callback: registering module adapter for '" << Name << "'\n";
                  MPM.addPass(createModuleToFunctionPassAdaptor(ControlFlowFlatteningPass()));
                  return true;
                }
                return false;
              });
          }};
}