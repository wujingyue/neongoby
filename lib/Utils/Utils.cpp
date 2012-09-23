// Author: Jingyue

#include <cassert>

#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/Utils.h"

using namespace llvm;
using namespace dyn_aa;

void DynAAUtils::PrintProgressBar(uint64_t Finished, uint64_t Total) {
  assert(Finished <= Total);
  assert(Total > 0);

  errs().changeColor(raw_ostream::BLUE);
  if (Finished == 0) {
    errs() << " [0%]";
  } else {
    unsigned CurrentPercentage = Finished * 10 / Total;
    unsigned OldPercentage = (Finished - 1) * 10 / Total;
    if (CurrentPercentage != OldPercentage)
      errs() << " [" << CurrentPercentage * 10 << "%]";
  }
  errs().resetColor();
}
