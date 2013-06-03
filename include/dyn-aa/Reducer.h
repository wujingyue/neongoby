// vim: sw=2

#ifndef __DYN_AA_REDUCER_H
#define __DYN_AA_REDUCER_H

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

typedef void (*ReductionFunction)(Module &);

using namespace llvm;
using namespace std;
using namespace rcs;

namespace dyn_aa {
struct Reducer: public ModulePass {
  static char ID;

  Reducer(): ModulePass(ID) {
    ReductionFunctions.push_back(Reducer::reduceUnexecuted);
    ReductionFunctions.push_back(Reducer::reduceUnrelatedFunctions);
    ReductionFunctions.push_back(Reducer::reduceGlobalVariables);
  }
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

  bool setReductionOptions(const vector<bool> &RO);

 private:
  static void reduceUnexecuted(Module &M);
  static void reduceUnrelatedFunctions(Module &M);
  static void reduceGlobalVariables(Module &M);
  static void reduceInstructions(Module &M);
  // indicate whether the ith reduction function is executed
  vector<bool> ReductionOptions;
  vector<ReductionFunction> ReductionFunctions;
};
}

#endif
