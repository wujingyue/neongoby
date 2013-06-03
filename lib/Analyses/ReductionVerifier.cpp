#define DEBUG_TYPE "dyn-aa"

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
#include "dyn-aa/Reducer.h"
#include "dyn-aa/ReductionVerifier.h"

using namespace std;
using namespace llvm;
using namespace dyn_aa;

static RegisterPass<ReductionVerifier> X("verify-reduction",
                                         "Verify whether reduction keeps "
                                         "the bug",
                                         false, // Is CFG Only?
                                         true); // Is Analysis?

char ReductionVerifier::ID = 0;

bool ReductionVerifier::runOnModule(Module &M) {
  Value *V[2];
  unsigned ValueNum = 0;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        if (I->getMetadata("alias")) {
          assert(ValueNum < 2);
          DbgDeclareInst *DDI = dyn_cast<DbgDeclareInst>(I);
          if (DDI) {
            V[ValueNum] = DDI->getAddress();
          } else {
            V[ValueNum] = I;
          }
          ValueNum++;
        }
      }
    }
  }
  assert(ValueNum == 2);

  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  Verified = AA.alias(V[0], V[1]) == AliasAnalysis::NoAlias;

  errs() << "Reduction Verifier: ";
  if (Verified) {
    errs() << "Verified\n";
  } else {
    errs() << "Not verified\n";
  }

  return false;
}

void ReductionVerifier::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AliasAnalysis>();
}
