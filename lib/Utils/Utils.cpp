// Author: Jingyue

#include <cassert>

#include "llvm/Argument.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/Utils.h"

using namespace llvm;
using namespace dyn_aa;

void DynAAUtils::PrintProgressBar(uint64_t Old, uint64_t Now, uint64_t Total) {
  assert(Total > 0);
  assert(Now <= Total);

  if (Now == 0) {
    errs().changeColor(raw_ostream::BLUE);
    errs() << " [0%]";
    errs().resetColor();
  } else {
    unsigned CurrentPercentage = Now * 10 / Total;
    unsigned OldPercentage = Old * 10 / Total;
    for (unsigned Percentage = OldPercentage + 1;
         Percentage <= CurrentPercentage; ++Percentage) {
      errs().changeColor(raw_ostream::BLUE);
      errs() << " [" << Percentage * 10 << "%]";
      errs().resetColor();
    }
  }
}

bool DynAAUtils::PointerIsDereferenced(const Value *V) {
  assert(V->getType()->isPointerTy());
  if (isa<Function>(V)) {
    // We always consider missing call edges important.
    return true;
  }
  for (Value::const_use_iterator UI = V->use_begin();
       UI != V->use_end(); ++UI) {
    if (const LoadInst *LI = dyn_cast<LoadInst>(*UI)) {
      if (LI->getPointerOperand() == V)
        return true;
    }
    if (const StoreInst *SI = dyn_cast<StoreInst>(*UI)) {
      if (SI->getPointerOperand() == V)
        return true;
    }
    ImmutableCallSite CS(*UI);
    if (CS && CS.getCalledValue() == V) {
      // Return true if V is used as a callee.
      return true;
    }
  }
  return false;
}

void DynAAUtils::PrintValue(raw_ostream &O, const Value *V) {
  if (isa<Function>(V)) {
    O << V->getName();
  } else if (const Argument *Arg = dyn_cast<Argument>(V)) {
    O << Arg->getParent()->getName() << ":  " << *Arg;
  } else if (const Instruction *Ins = dyn_cast<Instruction>(V)) {
    O << Ins->getParent()->getParent()->getName() << ":";
    O << *Ins;
  } else {
    O << *V;
  }
}
