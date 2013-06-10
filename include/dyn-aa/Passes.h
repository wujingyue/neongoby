#ifndef __DYN_AA_PASSES_H
#define __DYN_AA_PASSES_H

#include "llvm/Pass.h"

namespace neongoby {
llvm::ModulePass *createMemoryInstrumenterPass();
}

#endif
