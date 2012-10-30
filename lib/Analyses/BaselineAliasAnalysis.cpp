// Author: Jingyue

#include <string>

#include "llvm/Support/CommandLine.h"

#include "dyn-aa/BaselineAliasAnalysis.h"

using namespace std;
using namespace llvm;
using namespace dyn_aa;

static RegisterPass<BaselineAliasAnalysis> X(
    "baseline-aa",
    "Baseline AA which is assumed to be correct",
    false,
    true);

static cl::opt<string> BaselineAAName("baseline-aa-name",
                                      cl::desc("the name of the baseline AA"),
                                      cl::init("no-aa"));

char BaselineAliasAnalysis::ID = 0;

BaselineAliasAnalysis::BaselineAliasAnalysis(): ModulePass(ID), AA(NULL) {}

bool BaselineAliasAnalysis::runOnModule(Module &M) {
  // Get the baseline AA.
  // Don't use getAnalysisID directly. The getAdjustedAnalysisPointer won't
  // work with <BaselineAA>::ID.
  // The following code repeats the logic in getAnalysisID except that it
  // passes AliasAnalysis::ID to getAdjustedAnalysisPointer.
  const PassInfo *PI = lookupPassInfo(BaselineAAName);
  assert(PI && "The baseline AA is not registered");
  Pass *P = getResolver()->findImplPass(PI->getTypeInfo());
  assert(P);
  AA = (AliasAnalysis *)P->getAdjustedAnalysisPointer(&AliasAnalysis::ID);

  // Get the current AA.
  AliasAnalysis *CurrentAA = &getAnalysis<AliasAnalysis>();
  assert(AA != CurrentAA &&
         "The baseline AA and the current AA should be different");

  return false;
}

void BaselineAliasAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AliasAnalysis>();
  // Find the baseline AA, and acquire its ID.
  const PassInfo *PI = lookupPassInfo(BaselineAAName);
  if (!PI) {
    errs() << "Baseline " << BaselineAAName << " is not registered\n";
    assert(false && "The baseline AA is not registered");
  }
  AU.addRequiredID(PI->getTypeInfo());
  AU.setPreservesAll();
}

void *BaselineAliasAnalysis::getAdjustedAnalysisPointer(AnalysisID PI) {
  if (PI == &AliasAnalysis::ID)
    return (AliasAnalysis *)this;
  return this;
}

AliasAnalysis::AliasResult BaselineAliasAnalysis::alias(const Location &L1,
                                                        const Location &L2) {
  assert(AA && "Haven't retrieved the baseline AA");
  return AA->alias(L1, L2);
}
