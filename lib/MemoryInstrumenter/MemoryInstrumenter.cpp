/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "instrument-memory"

#include "llvm/Module.h"
#include "llvm/Pass.h"
using namespace llvm;

namespace dyn_aa {
  struct MemoryInstrumenter: public FunctionPass {
    static char ID;

    MemoryInstrumenter(): FunctionPass(ID) {}
    virtual bool runOnFunction(Function &F);
  };
}
using namespace dyn_aa;

char MemoryInstrumenter::ID = 0;

static RegisterPass<MemoryInstrumenter> X("instrument-memory",
    "Instrument memory operations",
    false, false);

bool MemoryInstrumenter::runOnFunction(Function &F) {
  return true;
}
