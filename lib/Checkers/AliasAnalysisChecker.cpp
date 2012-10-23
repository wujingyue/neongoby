// Author: Jingyue
//
// Checks whether a specified call graph is sound by comparing it with another
// call graph generated on DynamicAliasAnalysis

#include <string>
#include <fstream>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include "rcs/typedefs.h"
#include "rcs/IDAssigner.h"

#include "dyn-aa/DynamicAliasAnalysis.h"
#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

namespace dyn_aa {
struct AliasAnalysisChecker: public ModulePass {
  static char ID;

  AliasAnalysisChecker(): ModulePass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

 private:
  static bool IsIntraProcQuery(const Value *V1, const Value *V2);
  static const Function *GetContainingFunction(const Value *V);
};
}

static cl::opt<bool> IntraProc(
    "intra",
    cl::desc("Whether the checked AA supports intra-procedural queries only"));
static cl::opt<string> InputDynamicAliases(
    "input-dyn-aliases",
    cl::desc("Input dynamic aliases"));

static RegisterPass<AliasAnalysisChecker> X(
    "check-aa",
    "Check whether the alias analysis is sound",
    false, // Is CFG Only?
    true); // Is Analysis?

char AliasAnalysisChecker::ID = 0;

void AliasAnalysisChecker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  // Note that DynamicAliasAnalysis is not registered to the
  // AliasAnalysis group.
  if (InputDynamicAliases == "") {
    AU.addRequired<DynamicAliasAnalysis>();
  }
  AU.addRequired<AliasAnalysis>();
  AU.addRequired<IDAssigner>();
}

bool AliasAnalysisChecker::runOnModule(Module &M) {
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  DenseSet<ValuePair> DynamicAliases;
  if (InputDynamicAliases == "") {
    DynamicAliasAnalysis &DAA = getAnalysis<DynamicAliasAnalysis>();
    DynamicAliases.insert(DAA.getAllAliases().begin(),
                          DAA.getAllAliases().end());
  } else {
    ifstream InputFile(InputDynamicAliases.c_str());
    unsigned VID1, VID2;
    while (InputFile >> VID1 >> VID2) {
      Value *V1 = IDA.getValue(VID1), *V2 = IDA.getValue(VID2);
      DynamicAliases.insert(make_pair(V1, V2));
    }
  }

  unsigned NumMissingAliases = 0;
  for (DenseSet<ValuePair>::iterator I = DynamicAliases.begin();
       I != DynamicAliases.end(); ++I) {
    Value *V1 = I->first, *V2 = I->second;
    if (IntraProc && !IsIntraProcQuery(V1, V2)) {
      continue;
    }
    if (!DynAAUtils::PointerIsAccessed(V1) ||
        !DynAAUtils::PointerIsAccessed(V2)) {
      continue;
    }
    if (AA.alias(V1, V2) == AliasAnalysis::NoAlias) {
      ++NumMissingAliases;

      errs().changeColor(raw_ostream::RED);
      errs() << "Missing alias:\n";
      errs().resetColor();

      errs() << "[" << IDA.getValueID(V1) << "] ";
      DynAAUtils::PrintValue(errs(), V1);
      errs() << "\n";

      errs() << "[" << IDA.getValueID(V2) << "] ";
      DynAAUtils::PrintValue(errs(), V2);
      errs() << "\n";
    }
  }

  if (NumMissingAliases == 0) {
    errs().changeColor(raw_ostream::GREEN, true);
    errs() << "Congrats! You passed all the tests.\n";
    errs().resetColor();
  } else {
    errs().changeColor(raw_ostream::RED, true);
    errs() << "Detected " << NumMissingAliases << " missing aliases.\n";
    errs().resetColor();
  }

  return false;
}

bool AliasAnalysisChecker::IsIntraProcQuery(const Value *V1, const Value *V2) {
  assert(V1->getType()->isPointerTy() && V2->getType()->isPointerTy());
  const Function *F1 = GetContainingFunction(V1);
  const Function *F2 = GetContainingFunction(V2);
  return F1 == NULL || F2 == NULL || F1 == F2;
}

const Function *AliasAnalysisChecker::GetContainingFunction(const Value *V) {
  if (const Instruction *Ins = dyn_cast<Instruction>(V))
    return Ins->getParent()->getParent();
  if (const Argument *Arg = dyn_cast<Argument>(V))
    return Arg->getParent();
  return NULL;
}
