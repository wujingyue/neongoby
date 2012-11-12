// Author: Jingyue

#include <string>
#include <fstream>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include "rcs/typedefs.h"
#include "rcs/IDAssigner.h"

#include "dyn-aa/BaselineAliasAnalysis.h"
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
  static bool CompareMissingAliases(const ValuePair &VP1, const ValuePair &VP2);
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
                                           cl::desc("input dynamic aliases"));
static cl::opt<bool> CheckAllPointers("check-all-pointers",
                                      cl::desc("check all pointers"));
static cl::opt<bool> PrintValueInReport("print-value-in-report",
                                        cl::desc("print values in the report"),
                                        cl::init(true));

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
  AU.addRequired<BaselineAliasAnalysis>();
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
  AliasAnalysis &BaselineAA = getAnalysis<BaselineAliasAnalysis>();

  MissingAliases.clear();
  for (DenseSet<ValuePair>::const_iterator I = DynamicAliases.begin();
       I != DynamicAliases.end(); ++I) {
    Value *V1 = I->first, *V2 = I->second;
    if (IntraProc && !DynAAUtils::IsIntraProcQuery(V1, V2)) {
      continue;
    }

    // Ignore BitCasts and PhiNodes. The reports on them are typically
    // redundant.
    if (isa<BitCastInst>(V1) || isa<BitCastInst>(V2))
      continue;
    if (isa<PHINode>(V1) || isa<PHINode>(V2))
      continue;

    if (!CheckAllPointers) {
      if (!DynAAUtils::PointerIsDereferenced(V1) ||
          !DynAAUtils::PointerIsDereferenced(V2)) {
        continue;
      }
    }

    if (BaselineAA.alias(V1, V2) != AliasAnalysis::NoAlias &&
        AA.alias(V1, V2) == AliasAnalysis::NoAlias) {
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
    errs() << "Missing alias:";
    errs().resetColor();

    errs() << (DynAAUtils::IsIntraProcQuery(V1, V2) ? " (intra)" : " (inter)");
    if (DynAAUtils::PointerIsDereferenced(V1) &&
        DynAAUtils::PointerIsDereferenced(V2)) {
      errs() << " (deref)";
    } else {
      errs() << " (non-deref)";
    }
    errs() << "\n";

    errs() << "[" << IDA.getValueID(V1) << "] ";
    if (PrintValueInReport)
      DynAAUtils::PrintValue(errs(), V1);
    errs() << "\n";

    errs() << "[" << IDA.getValueID(V2) << "] ";
    if (PrintValueInReport)
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

pair<Function *, Function *> AliasAnalysisChecker::GetContainingFunctionPair(
    const ValuePair &VP) {
  const Function *F1 = DynAAUtils::GetContainingFunction(VP.first);
  const Function *F2 = DynAAUtils::GetContainingFunction(VP.second);
  return make_pair(const_cast<Function *>(F1), const_cast<Function *>(F2));
}
