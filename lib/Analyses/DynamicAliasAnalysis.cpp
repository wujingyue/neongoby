// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <cstdio>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/DynamicAliasAnalysis.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

static RegisterPass<DynamicAliasAnalysis> X("dyn-aa",
                                            "Accurate alias analysis "
                                            "according to the point-to log",
                                            false, // Is CFG Only?
                                            true); // Is Analysis?
// Don't register to AliasAnalysis group. It would confuse CallGraphChecker
// to use DynamicAliasAnalysis as the underlying AA to generate the call
// graph.
// static RegisterAnalysisGroup<AliasAnalysis> Y(X);

static cl::opt<string> OutputDynamicAliases(
    "output-dyn-aliases",
    cl::desc("Dump all dynamic aliases"));

STATISTIC(NumRemoveOps, "Number of remove operations");
STATISTIC(NumInsertOps, "Number of insert operations");

char DynamicAliasAnalysis::ID = 0;

const unsigned DynamicAliasAnalysis::UnknownVersion = (unsigned)-1;

bool DynamicAliasAnalysis::runOnModule(Module &M) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  InitializeAliasAnalysis(this);

  processLog();

  errs() << "# of aliases = " << Aliases.size() << "\n";
  if (OutputDynamicAliases != "") {
    string ErrorInfo;
    raw_fd_ostream OutputFile(OutputDynamicAliases.c_str(), ErrorInfo);
    for (DenseSet<ValuePair>::iterator I = Aliases.begin();
         I != Aliases.end(); ++I) {
      Value *V1 = I->first, *V2 = I->second;
      OutputFile << IDA.getValueID(V1) << " " << IDA.getValueID(V2) << "\n";
    }
  }

#if 0
  errs() << PointersVersionUnknown.size()
      << " pointers whose version is unknown:\n";
  for (ValueSet::iterator I = PointersVersionUnknown.begin();
       I != PointersVersionUnknown.end(); ++I) {
    IDA.printValue(errs(), *I);
    errs() << "\n";
  }
#endif

  return false;
}

void DynamicAliasAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AliasAnalysis::getAnalysisUsage(AU);
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
}

void DynamicAliasAnalysis::updateVersion(void *Start,
                                         unsigned long Bound,
                                         unsigned Version) {
  Interval I((unsigned long)Start, (unsigned long)Start + Bound);
  pair<IntervalTree<unsigned>::iterator, IntervalTree<unsigned>::iterator> ER =
      AddressVersion.equal_range(I);
  AddressVersion.erase(ER.first, ER.second);
  AddressVersion.insert(make_pair(I, Version));
}

void DynamicAliasAnalysis::processAddrTakenDecl(
    const AddrTakenDeclLogRecord &Record) {
  updateVersion(Record.Address, Record.Bound, CurrentVersion);
  ++CurrentVersion;
  // Check for numeric overflow.
  assert(CurrentVersion != UnknownVersion);
}

void DynamicAliasAnalysis::processTopLevelPointTo(
    const TopLevelPointToLogRecord &Record) {
  unsigned PointerVID = Record.PointerValueID;
  void *PointeeAddress = Record.PointeeAddress;

  // No matter the new address is NULL or not, remove the original mapping.
  removePointingTo(PointerVID);

  // We don't consider NULLs as aliases.
  if (PointeeAddress != NULL) {
    unsigned Version = lookupAddress(PointeeAddress);

    // Report the pointers pointing to unversioned address.
    if (Version == UnknownVersion) {
      if (!AddressesVersionUnknown.count(PointeeAddress)) {
        AddressesVersionUnknown.insert(PointeeAddress);
#if 0
        errs() << "Unknown version: " << PointerVID << " => "
            << PointeeAddress << "\n";
#endif
        IDAssigner &IDA = getAnalysis<IDAssigner>();
        PointersVersionUnknown.insert(IDA.getValue(PointerVID));
      }
    }

    addPointingTo(PointerVID, PointeeAddress, Version);

    // Report aliases.
    PointedByMapTy::iterator I = BeingPointedBy.find(
        make_pair(PointeeAddress, Version));
    assert(I != BeingPointedBy.end()); // We just added a point-to in.
    addAliasPairs(PointerVID, I->second);
  }
}

void DynamicAliasAnalysis::removePointingTo(unsigned ValueID) {
  ++NumRemoveOps;
  if (ValueID < PointingTo.size() && PointingTo[ValueID].first != NULL) {
    // Remove from BeingPointedBy.
    PointedByMapTy::iterator J = BeingPointedBy.find(PointingTo[ValueID]);
    assert(J != BeingPointedBy.end());
    vector<unsigned>::iterator K = find(J->second.begin(),
                                        J->second.end(),
                                        ValueID);
    assert(K != J->second.end());
    J->second.erase(K);

    // Remove from PointingTo.
    PointingTo[ValueID].first = NULL;
    PointingTo[ValueID].second = UnknownVersion;
  }
}

void DynamicAliasAnalysis::addPointingTo(unsigned ValueID,
                                         void *Address,
                                         unsigned Version) {
  ++NumInsertOps;
  if (ValueID >= PointingTo.size())
    PointingTo.resize(ValueID + 1);
  PointingTo[ValueID].first = Address;
  PointingTo[ValueID].second = Version;
  BeingPointedBy[make_pair(Address, Version)].push_back(ValueID);
}

unsigned DynamicAliasAnalysis::lookupAddress(void *Addr) const {
  Interval I((unsigned long)Addr, (unsigned long)Addr + 1);
  IntervalTree<unsigned>::const_iterator Pos = AddressVersion.find(I);
  if (Pos == AddressVersion.end())
    return UnknownVersion;
  return Pos->second;
}

void *DynamicAliasAnalysis::getAdjustedAnalysisPointer(AnalysisID PI) {
  if (PI == &AliasAnalysis::ID)
    return (AliasAnalysis *)this;
  return this;
}

AliasAnalysis::AliasResult DynamicAliasAnalysis::alias(const Location &L1,
                                                       const Location &L2) {
  Value *V1 = const_cast<Value *>(L1.Ptr);
  Value *V2 = const_cast<Value *>(L2.Ptr);
  if (V1 > V2)
    swap(V1, V2);
  if (Aliases.count(make_pair(V1, V2)))
    return MayAlias;
  return NoAlias;
}

void DynamicAliasAnalysis::addAliasPair(Value *V1, Value *V2) {
  assert(V1 && V2);
  if (V1 > V2)
    swap(V1, V2);
  Aliases.insert(make_pair(V1, V2));
}

void DynamicAliasAnalysis::addAliasPair(unsigned VID1, unsigned VID2) {
  assert(VID1 != IDAssigner::InvalidID && VID2 != IDAssigner::InvalidID);
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  addAliasPair(IDA.getValue(VID1), IDA.getValue(VID2));
}

void DynamicAliasAnalysis::addAliasPairs(unsigned VID1,
                                         const vector<unsigned> &VID2s) {
  for (size_t j = 0; j < VID2s.size(); ++j)
    addAliasPair(VID1, VID2s[j]);
}
