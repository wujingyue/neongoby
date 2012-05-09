// Author: Jingyue
//
// Checks whether a specified call graph is sound by comparing it with another
// call graph generated on DynamicAliasAnalysis

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include "common/typedefs.h"

#include "dyn-aa/DynamicAliasAnalysis.h"

using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

namespace dyn_aa {
struct AliasAnalysisChecker: public ModulePass {
  static char ID;

  AliasAnalysisChecker(): ModulePass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

 private:
  static void PrintValue(raw_ostream &O, const Value *V);
  static bool isIntraProcQuery(const Value *V1, const Value *V2);
  static const Function *getContainingFunction(const Value *V);
};
}

static cl::opt<bool> IntraProc(
    "intra",
    cl::desc("Whether the checked AA supports intra-procedural queries only"));

static RegisterPass<AliasAnalysisChecker> X(
    "check-aa",
    "Check whether the alias analysis is sound",
    false, // Is CFG Only?
    true); // Is Analysis?

char AliasAnalysisChecker::ID = 0;

void AliasAnalysisChecker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  // Note that DynamicAliasAnalysis is not registered to the
  // AliasAnalysis group.
  AU.addRequired<DynamicAliasAnalysis>();
  AU.addRequired<AliasAnalysis>();
}

bool AliasAnalysisChecker::runOnModule(Module &M) {
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  AliasAnalysis &DAA = getAnalysis<DynamicAliasAnalysis>();

  // We don't want to include all pointers yet.
  // For now, we include all pointers used as a pointer operand of
  // a Load/Store instruction.
  ValueSet PointerOperands;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
        if (StoreInst *SI = dyn_cast<StoreInst>(Ins))
          PointerOperands.insert(SI->getPointerOperand());
        if (LoadInst *LI = dyn_cast<LoadInst>(Ins))
          PointerOperands.insert(LI->getPointerOperand());
      }
    }
  }
  errs() << "# of pointer operands to process = "
      << PointerOperands.size() << "\n";

  unsigned NumMissingAliases = 0;
  for (ValueSet::iterator I = PointerOperands.begin();
       I != PointerOperands.end(); ++I) {
    Value *V1 = *I;
    ValueSet::iterator J = I;
    for (++J; J != PointerOperands.end(); ++J) {
      Value *V2 = *J;
      // If the checked AA supports only intra-procedural queries,
      // and V1 and V2 belong to different functions, skip this query.
      if (IntraProc && !isIntraProcQuery(V1, V2))
        continue;
      if (AA.alias(V1, V2) == AliasAnalysis::NoAlias &&
          DAA.alias(V1, V2) != AliasAnalysis::NoAlias) {
        ++NumMissingAliases;
        errs().changeColor(raw_ostream::RED);
        errs() << "Missing alias:\n";
        errs().resetColor();
        PrintValue(errs(), V1); errs() << "\n";
        PrintValue(errs(), V2); errs() << "\n";
      }
    }
  }

  if (NumMissingAliases == 0) {
    errs().changeColor(raw_ostream::GREEN, true);
    errs() << "Congrats! You passed all the tests.\n";
    errs().resetColor();
  } else {
    errs().changeColor(raw_ostream::RED, true);
    errs() << "Detected " << NumMissingAliases << " missing aliases.\n";
    errs().resetColor();
  }

  return false;
}

void AliasAnalysisChecker::PrintValue(raw_ostream &O, const Value *V) {
  if (isa<Function>(V)) {
    O << V->getName();
  } else if (const Argument *Arg = dyn_cast<Argument>(V)) {
    O << Arg->getParent()->getName() << ":  " << *Arg;
  } else if (const Instruction *Ins = dyn_cast<Instruction>(V)) {
    O << Ins->getParent()->getParent()->getName();
    O << "." << Ins->getParent()->getName() << ":";
    O << *Ins;
  } else {
    O << *V;
  }
}

bool AliasAnalysisChecker::isIntraProcQuery(const Value *V1, const Value *V2) {
  assert(V1->getType()->isPointerTy() && V2->getType()->isPointerTy());
  const Function *F1 = getContainingFunction(V1);
  const Function *F2 = getContainingFunction(V2);
  return F1 == NULL || F2 == NULL || F1 == F2;
}

const Function *AliasAnalysisChecker::getContainingFunction(const Value *V) {
  if (const Instruction *Ins = dyn_cast<Instruction>(V))
    return Ins->getParent()->getParent();
  if (const Argument *Arg = dyn_cast<Argument>(V))
    return Arg->getParent();
  return NULL;
}
