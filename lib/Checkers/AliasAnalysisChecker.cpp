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
  static bool CompareMissingAliases(const ValuePair &VP1, const ValuePair &VP2);
  static const Function *GetContainingFunction(const Value *V);
  static pair<Function *, Function *> GetContainingFunctionPair(
      const ValuePair &VP);

  void collectDynamicAliases(DenseSet<ValuePair> &DynamicAliases);
  void collectMissingAliases(const DenseSet<ValuePair> &DynamicAliases,
                             vector<ValuePair> &MissingAliases);
  void sortMissingAliases(vector<ValuePair> &MissingAliases);
  void reportMissingAliases(const vector<ValuePair> &MissingAliases);
};
}

static cl::opt<bool> IntraProc(
    "intra",
    cl::desc("Whether the checked AA supports intra-procedural queries only"));
static cl::opt<string> InputDynamicAliases("input-dyn-aliases",
                                           cl::desc("Input dynamic aliases"));
static cl::opt<bool> CheckAllPointers("check-all-pointers",
                                      cl::desc("Check all pointers"));

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

void AliasAnalysisChecker::collectDynamicAliases(
    DenseSet<ValuePair> &DynamicAliases) {
  DynamicAliases.clear();
  if (InputDynamicAliases == "") {
    DynamicAliasAnalysis &DAA = getAnalysis<DynamicAliasAnalysis>();
    DynamicAliases.insert(DAA.getAllAliases().begin(),
                          DAA.getAllAliases().end());
  } else {
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    ifstream InputFile(InputDynamicAliases.c_str());
    unsigned VID1, VID2;
    while (InputFile >> VID1 >> VID2) {
      Value *V1 = IDA.getValue(VID1), *V2 = IDA.getValue(VID2);
      DynamicAliases.insert(make_pair(V1, V2));
    }
  }
}

void AliasAnalysisChecker::collectMissingAliases(
    const DenseSet<ValuePair> &DynamicAliases,
    vector<ValuePair> &MissingAliases) {
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();

  MissingAliases.clear();
  for (DenseSet<ValuePair>::const_iterator I = DynamicAliases.begin();
       I != DynamicAliases.end(); ++I) {
    Value *V1 = I->first, *V2 = I->second;
    if (IntraProc && !IsIntraProcQuery(V1, V2)) {
      continue;
    }
    if (!CheckAllPointers) {
      if (!DynAAUtils::PointerIsDereferenced(V1) ||
          !DynAAUtils::PointerIsDereferenced(V2)) {
        continue;
      }
    }
    if (CheckAllPointers) {
      if (isa<BitCastInst>(V1) || isa<BitCastInst>(V2))
        continue;
      if (isa<PHINode>(V1) || isa<PHINode>(V2))
        continue;
    }
    if (AA.alias(V1, V2) == AliasAnalysis::NoAlias) {
      MissingAliases.push_back(make_pair(V1, V2));
    }
  }
}

void AliasAnalysisChecker::reportMissingAliases(
    const vector<ValuePair> &MissingAliases) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  for (size_t i = 0; i < MissingAliases.size(); ++i) {
    Value *V1 = MissingAliases[i].first, *V2 = MissingAliases[i].second;
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

  if (MissingAliases.empty()) {
    errs().changeColor(raw_ostream::GREEN, true);
    errs() << "Congrats! You passed all the tests.\n";
    errs().resetColor();
  } else {
    errs().changeColor(raw_ostream::RED, true);
    errs() << "Detected " << MissingAliases.size() << " missing aliases.\n";
    errs().resetColor();
  }
}

bool AliasAnalysisChecker::CompareMissingAliases(const ValuePair &VP1,
                                                 const ValuePair &VP2) {
  pair<Function *, Function *> FP1 = GetContainingFunctionPair(VP1);
  pair<Function *, Function *> FP2 = GetContainingFunctionPair(VP2);
  return FP1 < FP2;
}

void AliasAnalysisChecker::sortMissingAliases(
    vector<ValuePair> &MissingAliases) {
  sort(MissingAliases.begin(), MissingAliases.end(), CompareMissingAliases);
}

bool AliasAnalysisChecker::runOnModule(Module &M) {
  DenseSet<ValuePair> DynamicAliases;
  collectDynamicAliases(DynamicAliases);

  vector<ValuePair> MissingAliases;
  collectMissingAliases(DynamicAliases, MissingAliases);

  sortMissingAliases(MissingAliases);

  reportMissingAliases(MissingAliases);

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

pair<Function *, Function *> AliasAnalysisChecker::GetContainingFunctionPair(
    const ValuePair &VP) {
  return make_pair(const_cast<Function *>(GetContainingFunction(VP.first)),
                   const_cast<Function *>(GetContainingFunction(VP.first)));
}
