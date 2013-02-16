#ifndef __DYN_AA_DYNAMIC_ALIAS_ANALYSIS_H
#define __DYN_AA_DYNAMIC_ALIAS_ANALYSIS_H

#include <map>
#include <stack>

#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "rcs/typedefs.h"

#include "dyn-aa/IntervalTree.h"
#include "dyn-aa/LogRecord.h"
#include "dyn-aa/LogProcessor.h"

using namespace llvm;

namespace dyn_aa {
struct DynamicAliasAnalysis: public ModulePass,
                             public AliasAnalysis,
                             public LogProcessor {
  typedef std::pair<void *, unsigned> AddressTy;
  typedef std::pair<unsigned, unsigned> PointerTy;
  typedef DenseMap<AddressTy, DenseSet<PointerTy> > PointedByMapTy;
  typedef DenseMap<PointerTy, AddressTy> PointsToMapTy;

  static char ID;
  static const unsigned UnknownVersion;

  DynamicAliasAnalysis(): ModulePass(ID) {}
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  // Interfaces of AliasAnalysis.
  AliasAnalysis::AliasResult alias(const Location &L1, const Location &L2);
  virtual void *getAdjustedAnalysisPointer(AnalysisID PI);

  // Interfaces of LogProcessor.
  // TODO: use override keyward
  void processMemAlloc(const MemAllocRecord &Record);
  void processTopLevel(const TopLevelRecord &Record);
  void processEnter(const EnterRecord &Record);
  void processReturn(const ReturnRecord &Record);
  void initialize();

  const DenseSet<rcs::ValuePair> &getAllAliases() const { return Aliases; }

 private:
  // Returns the current version of <Addr>.
  unsigned lookupAddress(void *Addr) const;
  void updateVersion(void *Start, unsigned long Bound, unsigned Version);
  void removePointsTo(unsigned InvocationID);
  void removePointsTo(PointerTy Ptr);
  // Helper function called by removePointsTo.
  void removePointedBy(PointerTy Ptr, AddressTy Loc);
  void addPointsTo(PointerTy Ptr, AddressTy Loc);
  // A convenient wrapper for a batch of reports.
  void addAliasPairs(PointerTy P, const DenseSet<PointerTy> &Qs);
  // Adds two values to DidAlias if their contexts match.
  void addAliasPair(PointerTy P, PointerTy Q);
  // Report <V1, V2> as an alias pair.
  // This function canonicalizes the pair, so that <V1, V2> and
  // <V2, V1> are considered the same.
  void addAliasPair(Value *V1, Value *V2);

  // An interval tree that maps addresses to version numbers.
  // We need store version numbers because pointing to the same address is
  // not enough to claim two pointers alias.
  std::map<Interval, unsigned> AddressVersion;
  unsigned CurrentVersion;
  // 2-way mapping indicating the current address of each pointer
  PointedByMapTy PointedBy;
  PointsToMapTy PointsTo;
  // Stores all alias pairs.
  DenseSet<rcs::ValuePair> Aliases;
  // Pointers that ever point to unversioned addresses.
  rcs::ValueSet PointersVersionUnknown;
  // Addresses whose version is unknown.
  DenseSet<void *> AddressesVersionUnknown;
  // This global variable gets incremented each time a function is called. Each
  // pointer will be associated with the invocation ID to gain
  // context-sensitivity.
  unsigned NumInvocations;
  // Thread-specific call stack.
  std::stack<unsigned> CallStack;
  // Pointers in PointsTo and PointedBy. Indexed by invocation ID so that
  // we can quickly find out what pointers to delete when exiting a function.
  DenseMap<unsigned, std::vector<unsigned> > ActivePointers;
};
}

#endif
