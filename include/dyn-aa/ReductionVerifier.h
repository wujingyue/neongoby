// vim: sw=2

#ifndef __DYN_AA_REDUCTION_VERIFIER_H
#define __DYN_AA_REDUCTION_VERIFIER_H

#include <cstdio>

#include "llvm/IntrinsicInst.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/BaselineAliasAnalysis.h"
#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;
using namespace dyn_aa;

namespace dyn_aa {
struct ReductionVerifier: public ModulePass {
  static char ID;

  ReductionVerifier(): ModulePass(ID) { }
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  bool getVerified() { return Verified; }
 private:
  bool Verified;
};
}

#endif
