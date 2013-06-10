#include <string>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/BaselineAliasAnalysis.h"
#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;
using namespace neongoby;

static RegisterPass<BaselineAliasAnalysis> X(
    "baseline-aa",
    "Baseline AA which is assumed to be correct",
    false,
    true);
static RegisterAnalysisGroup<AliasAnalysis> Y(X);

static cl::opt<string> BaselineAAName("baseline-aa-name",
                                      cl::desc("the name of the baseline AA"),
                                      cl::init("no-aa"));
static cl::opt<bool> BaselineIntraProc(
    "baseline-intra",
    cl::desc("Whether the baseline AA supports intra-procedural queries only"));

char BaselineAliasAnalysis::ID = 0;

BaselineAliasAnalysis::BaselineAliasAnalysis(): ImmutablePass(ID) {}

void BaselineAliasAnalysis::initializePass() {
  InitializeAliasAnalysis(this);
}

void BaselineAliasAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AliasAnalysis::getAnalysisUsage(AU);
}

void *BaselineAliasAnalysis::getAdjustedAnalysisPointer(AnalysisID PI) {
  if (PI == &AliasAnalysis::ID)
    return (AliasAnalysis *)this;
  return this;
}

AliasAnalysis::AliasResult BaselineAliasAnalysis::alias(const Location &L1,
                                                        const Location &L2) {
  const Value *V1 = L1.Ptr, *V2 = L2.Ptr;
  // Act as a guard. If we are about to pass an inter-procedural query to
  // the next AA in the chain which doesn't support that, BaselineAliasAnalysis
  // returns MayAlias.
  if (!DynAAUtils::IsIntraProcQuery(V1, V2) && BaselineIntraProc)
    return AliasAnalysis::MayAlias;
  return AliasAnalysis::alias(L1, L2);
}
