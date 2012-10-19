// Author: Jingyue

#include <cassert>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Type.h"
#include "llvm/Instructions.h"

#include "dyn-aa/Utils.h"

using namespace llvm;
using namespace dyn_aa;

void DynAAUtils::PrintProgressBar(uint64_t Finished, uint64_t Total) {
  assert(Total > 0);
  // Total is estimated, so it can sometimes be less than Finished.
  if (Finished > Total)
    return;

  if (Finished == 0) {
    errs().changeColor(raw_ostream::BLUE);
    errs() << " [0%]";
    errs().resetColor();
  } else {
    unsigned CurrentPercentage = Finished * 10 / Total;
    unsigned OldPercentage = (Finished - 1) * 10 / Total;
    for (unsigned Percentage = OldPercentage + 1;
         Percentage <= CurrentPercentage; ++Percentage) {
      errs().changeColor(raw_ostream::BLUE);
      errs() << " [" << Percentage * 10 << "%]";
      errs().resetColor();
    }
  }
}

bool DynAAUtils::PointerIsAccessed(const Value *V) {
  assert(V->getType()->isPointerTy());
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
  }
  return false;
}
