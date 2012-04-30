// Author: Jingyue
//
// Checks whether a specified call graph is sound by comparing it with another
// call graph generated on DynamicAliasAnalysis

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "dyn-aa/DynamicPointerAnalysis.h"
using namespace dyn_aa;

namespace dyn_aa {
struct CallGraphChecker: public ModulePass {
  static char ID;

  CallGraphChecker(): ModulePass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

 private:
  // Look for the call edge from <Call> to <Callee> in the call graph of
  // <Call>'s containing function.
  // The current implementation iterates through all call edges from <Call>'s
  // containing function, which is a little bit slow.
  bool existsInCallGraph(Instruction *Call, Function *Callee);
};
}

static RegisterPass<CallGraphChecker> X("check-cg",
                                        "Check whether the call graph is "
                                        "sound by comparing it with "
                                        "DynamicPointerAnalysis",
                                        false, // Is CFG Only?
                                        true); // Is Analysis?

char CallGraphChecker::ID = 0;

void CallGraphChecker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<DynamicPointerAnalysis>();
  AU.addRequired<CallGraph>();
}

bool CallGraphChecker::runOnModule(Module &M) {
  PointerAnalysis &PA = getAnalysis<DynamicPointerAnalysis>();

  unsigned NumMissingCallEdges = 0;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
        CallSite CS(Ins);
        if (CS && CS.getCalledFunction() == NULL) {
          Value *FP = CS.getCalledValue();
          ValueList Pointees;
          PA.getPointees(FP, Pointees);
          for (size_t j = 0; j < Pointees.size(); ++j) {
            Value *Pointee = Pointees[j];
            assert(Pointee);
            Function *Callee = dyn_cast<Function>(Pointee);
            assert(Callee);
            if (!existsInCallGraph(Ins, Callee)) {
              ++NumMissingCallEdges;
              errs().changeColor(raw_ostream::RED);
              errs() << "Call edge does not exist in the call graph:\n";
              errs().resetColor();
              errs() << *Ins << "\n";
              errs() << "  " << Callee->getName() << "\n";
            }
          }
        }
      }
    }
  }

  if (NumMissingCallEdges == 0) {
    errs().changeColor(raw_ostream::GREEN, true);
    errs() << "Congrats! You passed all the tests.\n";
    errs().resetColor();
  } else {
    errs().changeColor(raw_ostream::RED, true);
    errs() << "Detected " << NumMissingCallEdges << " missing call edges.\n";
    errs().resetColor();
  }

  return false;
}

bool CallGraphChecker::existsInCallGraph(Instruction *Call, Function *Callee) {
  CallGraph &CG = getAnalysis<CallGraph>();

  assert(Call && Callee);
  CallGraphNode *CallerNode = CG[Call->getParent()->getParent()];
  CallGraphNode *CalleeNode = CG[Callee];
  assert(CallerNode && CalleeNode);

  if (find(CallerNode->begin(), CallerNode->end(),
           CallGraphNode::CallRecord(Call, CalleeNode))
      != CallerNode->end()) {
    return true;
  }

  // An instruction conservatively calls all functions by calling
  // CallsExternalNode.
  if (find(CallerNode->begin(), CallerNode->end(),
           CallGraphNode::CallRecord(Call, CG.getCallsExternalNode()))
      != CallerNode->end()) {
    return true;
  }

  return false;
}
