#ifndef __DYN_AA_BASELINE_ALIAS_ANALYSIS_H
#define __DYN_AA_BASELINE_ALIAS_ANALYSIS_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"

using namespace llvm;

namespace neongoby {
// Make it an ImmutablePass so that it can be inserted before and after
// basicaa or other ImmutablePasses.
struct BaselineAliasAnalysis: public ImmutablePass, public AliasAnalysis {
  static char ID;

  BaselineAliasAnalysis();
  virtual void initializePass();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  virtual void *getAdjustedAnalysisPointer(AnalysisID PI);
  // TODO: implement other interfaces of AliasAnalysis
  AliasAnalysis::AliasResult alias(const Location &L1, const Location &L2);

 private:
  AliasAnalysis *AA;
};
}

#endif
