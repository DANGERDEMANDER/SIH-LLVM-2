#include "ControlFlowFlatteningPass.h" // Use the header for the declaration
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <random>
#include <vector>

using namespace llvm;

// NOTE: The incorrect 'namespace llvm { ... }' wrapper has been removed.
// The implementation is now in the global namespace, matching the header.

PreservedAnalyses ControlFlowFlatteningPass::run(Function &F, FunctionAnalysisManager &AM) {
    if (F.isDeclaration() || F.empty() || F.size() <= 2) {
        return PreservedAnalyses::all();
    }

    std::vector<BasicBlock*> origBBs;
    for (BasicBlock &BB : F) {
        origBBs.push_back(&BB);
    }
    
    BasicBlock *entryBlock = &F.getEntryBlock();
    origBBs.erase(std::remove(origBBs.begin(), origBBs.end(), entryBlock), origBBs.end());
    
    // Detach all original blocks from the function, except the entry block.
    for (BasicBlock *BB : origBBs) {
        BB->removeFromParent();
    }

    LLVMContext &Ctx = F.getContext();
    BasicBlock *dispatchBlock = BasicBlock::Create(Ctx, "dispatch", &F);
    BasicBlock *returnBlock = BasicBlock::Create(Ctx, "returnBlock", &F);

    // Setup the entry block to initialize the state and jump to the dispatcher
    entryBlock->getTerminator()->eraseFromParent();
    IRBuilder<> builder(entryBlock);
    AllocaInst *stateVar = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "cff_state");
    builder.CreateStore(builder.getInt32(1), stateVar); // Start with state 1
    builder.CreateBr(dispatchBlock);

    // Create the dispatcher's switch statement
    builder.SetInsertPoint(dispatchBlock);
    LoadInst *loadState = builder.CreateLoad(builder.getInt32Ty(), stateVar, "load_cff_state");
    SwitchInst *switcher = builder.CreateSwitch(loadState, returnBlock, origBBs.size());

    // Re-wire all original blocks to work with the dispatcher
    for (size_t i = 0; i < origBBs.size(); ++i) {
        BasicBlock *BB = origBBs[i];
        F.getBasicBlockList().push_back(BB); 
        switcher->addCase(builder.getInt32(i + 1), BB);

        Instruction *terminator = BB->getTerminator();
        if (isa<ReturnInst>(terminator)) {
            builder.SetInsertPoint(terminator);
            builder.CreateStore(builder.getInt32(0), stateVar); // State 0 can mean "exit"
            builder.CreateBr(returnBlock); // Jump to the common return block
            terminator->eraseFromParent();
        } else if (BranchInst *br = dyn_cast<BranchInst>(terminator)) {
            builder.SetInsertPoint(br);
            
            auto find_idx = [&](BasicBlock* target) {
                for(size_t j = 0; j < origBBs.size(); ++j) {
                    if(origBBs[j] == target) return (uint32_t)j + 1;
                }
                // It might branch back to the entry, which is not in our list. Handle that case.
                if(target == entryBlock) return (uint32_t)1;
                return (uint32_t)0; // Default/exit state
            };

            if (br->isConditional()) {
                Value* trueState = builder.getInt32(find_idx(br->getSuccessor(0)));
                Value* falseState = builder.getInt32(find_idx(br->getSuccessor(1)));
                Value* nextState = builder.CreateSelect(br->getCondition(), trueState, falseState);
                builder.CreateStore(nextState, stateVar);
            } else {
                builder.CreateStore(builder.getInt32(find_idx(br->getSuccessor(0))), stateVar);
            }
            builder.CreateBr(dispatchBlock); // Always go back to the dispatcher
            terminator->eraseFromParent();
        }
    }
    
    // If the switch gets an unknown state, go to the return block
    switcher->setDefaultDest(returnBlock);
    builder.SetInsertPoint(returnBlock);
    if (F.getReturnType()->isVoidTy()) {
        builder.CreateRetVoid();
    } else {
        // Return a default/undefined value if the function should return something
        builder.CreateRet(UndefValue::get(F.getReturnType()));
    }

    return PreservedAnalyses::none();
}