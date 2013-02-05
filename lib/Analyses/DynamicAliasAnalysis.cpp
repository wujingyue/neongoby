#define DEBUG_TYPE "dyn-aa"

#include <cstdio>

#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/DynamicAliasAnalysis.h"
#include "dyn-aa/Utils.h"

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
STATISTIC(MaxNumPointersToSameLocation,
          "Maximum number of pointers to the same location. "
          "Used for analyzing time complexity");

char DynamicAliasAnalysis::ID = 0;

const unsigned DynamicAliasAnalysis::UnknownVersion = (unsigned)-1;

bool DynamicAliasAnalysis::runOnModule(Module &M) {
  // We needn't chain DynamicAliasAnalysis to the AA chain
  // InitializeAliasAnalysis(this);

  IDAssigner &IDA = getAnalysis<IDAssigner>();

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
  // TODO: Do we need this? since DynamicAliasAnalysis is not in the AA chain
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
  assert(lookupAddress(Start) == Version);
}

void DynamicAliasAnalysis::processMemAlloc(const MemAllocRecord &Record) {
  updateVersion(Record.Address, Record.Bound, CurrentVersion);
  ++CurrentVersion;
  // Check for numeric overflow.
  assert(CurrentVersion != UnknownVersion);
}

void DynamicAliasAnalysis::processEnter(const EnterRecord &Record) {
  ++NumInvocations;
  CallStacks[getCurrentThreadID()].push(NumInvocations);
}

void DynamicAliasAnalysis::processReturn(const ReturnRecord &Record) {
  stack<unsigned> &CallStack = CallStacks[getCurrentThreadID()];
  assert(!CallStack.empty());
  removePointingTo(CallStack.top());
  CallStack.pop();
}

void DynamicAliasAnalysis::processTopLevel(const TopLevelRecord &Record) {
  unsigned PointerVID = Record.PointerValueID;
  void *PointeeAddress = Record.PointeeAddress;

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

    // Global variables are processed before any invocation.
    stack<unsigned> &CallStack = CallStacks[getCurrentThreadID()];
    PointerTy Ptr(PointerVID, CallStack.empty() ? 0 : CallStack.top());
    AddressTy Loc(PointeeAddress, Version);
    addPointingTo(Ptr, Loc);

    // Report aliases.
    PointedByMapTy::iterator I = BeingPointedBy.find(Loc);
    assert(I != BeingPointedBy.end()); // We just added a point-to in.
    if (Version != UnknownVersion &&
        I->second.size() > MaxNumPointersToSameLocation) {
      MaxNumPointersToSameLocation = I->second.size();
    }
    addAliasPairs(Ptr, I->second);
  } // if (PointerAddress != NULL)
}

void DynamicAliasAnalysis::removePointedBy(PointerTy Ptr, AddressTy Loc) {
  PointedByMapTy::iterator J = BeingPointedBy.find(Loc);
  assert(J != BeingPointedBy.end());
  vector<PointerTy>::iterator K = find(J->second.begin(),
                                       J->second.end(),
                                       Ptr);
  assert(K != J->second.end());
  J->second.erase(K);
}

void DynamicAliasAnalysis::removePointingTo(PointerTy Ptr) {
  PointsToMapTy::iterator I = PointingTo.find(Ptr);
  if (I != PointingTo.end()) {
    ++NumRemoveOps;
    removePointedBy(I->first, I->second);
    PointingTo.erase(I);
  }
}

void DynamicAliasAnalysis::removePointingTo(unsigned InvocationID) {
  DenseMap<unsigned, vector<unsigned> >::iterator I =
      ActivePointers.find(InvocationID);
  if (I != ActivePointers.end()) {
    for (size_t i = 0; i < I->second.size(); ++i)
      removePointingTo(PointerTy(I->second[i], InvocationID));
    ActivePointers.erase(I);
  }
}

void DynamicAliasAnalysis::addPointingTo(PointerTy Ptr, AddressTy Loc) {
  ++NumInsertOps;
  removePointingTo(Ptr);
  PointingTo[Ptr] = Loc;
  BeingPointedBy[Loc].push_back(Ptr);
  ActivePointers[Ptr.second].push_back(Ptr.first);
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

void DynamicAliasAnalysis::addAliasPair(PointerTy P, PointerTy Q) {
  assert(P.first != IDAssigner::InvalidID &&
         Q.first != IDAssigner::InvalidID);
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *U = IDA.getValue(P.first), *V = IDA.getValue(Q.first);
  if (DynAAUtils::GetContainingFunction(U) == DynAAUtils::GetContainingFunction(V) &&
      P.second != Q.second) {
    return;
  }
  addAliasPair(U, V);
}

void DynamicAliasAnalysis::addAliasPairs(PointerTy P,
                                         const vector<PointerTy> &Qs) {
  for (size_t j = 0; j < Qs.size(); ++j)
    addAliasPair(P, Qs[j]);
}
