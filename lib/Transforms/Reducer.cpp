// vim: sw=2

#define DEBUG_TYPE "dyn-aa"

#include <string>

#include "llvm/IntrinsicInst.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"

#include "rcs/typedefs.h"
#include "rcs/IDAssigner.h"

#include "dyn-aa/LogProcessor.h"
#include "dyn-aa/Utils.h"
#include "dyn-aa/Reducer.h"

using namespace llvm;
using namespace std;
using namespace rcs;
using namespace dyn_aa;

char Reducer::ID = 0;

static RegisterPass<Reducer> X("reduce-testcase", "Reduce testcase",
                               false, false);

void Reducer::getAnalysisUsage(AnalysisUsage &AU) const {
}

void Reducer::reduceUnrelatedFunctions(Module &M) {
  unsigned NumFunctions = 0, NumUnrelatedFunctions = 0;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration()) {
      ++NumFunctions;
      BasicBlock *BB = &(F->getEntryBlock());
      bool Related = false;
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        if (I->getMetadata("related")) {
          Related = true;
          break;
        }
      }
      if (!Related) {
        ++NumUnrelatedFunctions;
        // leads to linking errs
        F->deleteBody();
      }
    }
  }
  errs() << "# of total functions " << NumFunctions << "\n";
  errs() << "# of unrelated functions " << NumUnrelatedFunctions << "\n";
}

void Reducer::reduceUnexecuted(Module &M) {
  unsigned NumFunctions = 0, NumUnexecutedFunctions = 0;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration()) {
      ++NumFunctions;
      BasicBlock *BB = &(F->getEntryBlock());
      bool Executed = false;
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        if (I->getMetadata("executed")) {
          Executed = true;
          break;
        }
      }
      if (!Executed) {
        ++NumUnexecutedFunctions;
        F->replaceAllUsesWith(UndefValue::get(F->getType()));
        F->deleteBody();
      } else {
        // delete unexecuted basic blocks
        BasicBlock *UnreachableBB = BasicBlock::Create(F->getContext(), "", F);
        new UnreachableInst(UnreachableBB->getContext(), UnreachableBB);
        for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
          for (succ_iterator SI = succ_begin(BB), E = succ_end(BB); SI != E;
               ++SI) {
            bool Executed = false;
            for (BasicBlock::iterator I = (*SI)->begin(); I != (*SI)->end();
                 ++I) {
              if (I->getMetadata("executed")) {
                Executed = true;
                break;
              }
            }
            if (!Executed) {
              (*SI)->removePredecessor(BB);
              BB->getTerminator()->setSuccessor(SI.getSuccessorIndex(),
                                                UnreachableBB);
            }
          }
        }
      }
    }
  }
  errs() << "# of total functions " << NumFunctions << "\n";
  errs() << "# of unexecuted functions " << NumUnexecutedFunctions << "\n";
}

void Reducer::reduceGlobalVariables(Module &M) {
  // get related global variables
  DenseSet<Value *> RelatedGVs;
  Function *F = M.getFunction("main");
  BasicBlock *BB = &(F->getEntryBlock());
  for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
    if (I->getMetadata("slice")) {
      DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I);
      if (DDI) {
        RelatedGVs.insert(DDI->getAddress());
      }
    }
  }

  // remove unrelated global variables
  unsigned NumGVs = 0, NumDeletedGVs = 0;
  for (Module::global_iterator I = M.global_begin(), E = M.global_end();
       I != E; ++I) {
    NumGVs++;
    if (I->hasInitializer() && !RelatedGVs.count(I)) {
      NumDeletedGVs++;
      if (!I->use_empty())
        I->replaceAllUsesWith(UndefValue::get(I->getType()));
      I->setInitializer(0);
      I->setLinkage(GlobalValue::ExternalLinkage);
    }
  }
  errs() << "# of total global variables " << NumGVs << "\n";
  errs() << "# of deleted global vairables " << NumDeletedGVs << "\n";
}

void Reducer::reduceInstructions(Module &M) {
  // TODO: remained instruction should form new basic block, need to see whether PHI remains constant
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration()) {
      for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
        for (BasicBlock::iterator I = BB->begin(); I != BB->end();) {
          Instruction *Inst = I++;
          if (!Inst->getMetadata("slice") && !Inst->getMetadata("alias")) {
            if (!isa<TerminatorInst>(Inst) && !isa<LandingPadInst>(Inst)) {
              if (!Inst->getType()->isVoidTy())
                Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
              Inst->eraseFromParent();
            }
          }
        }
      }
    }
  }
}

bool Reducer::runOnModule(Module &M) {
  errs() << "Reducer: try " << ReductionOptions.size() << "\n";
  for (unsigned i = 0; i < ReductionOptions.size(); i++) {
    if (ReductionOptions[i]) {
      errs() << "Reduction " << (i + 1) << "\n";
      (*ReductionFunctions[i])(M);
    }
  }

  return true;
}

// return false if all stages have been performed
bool Reducer::setReductionOptions(const vector<bool> &RO) {
  ReductionOptions.clear();
  ReductionOptions.insert(ReductionOptions.begin(), RO.begin(), RO.end());
  return ReductionOptions.size() <= ReductionFunctions.size();
}
