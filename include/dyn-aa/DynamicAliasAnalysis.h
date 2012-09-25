// Author: Jingyue

#ifndef __DYN_AA_DYNAMIC_ALIAS_ANALYSIS_H
#define __DYN_AA_DYNAMIC_ALIAS_ANALYSIS_H

#include <map>

#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "rcs/typedefs.h"

#include "dyn-aa/IntervalTree.h"
#include "dyn-aa/LogRecord.h"
#include "dyn-aa/LogProcessor.h"

using namespace llvm;

namespace dyn_aa {
struct DynamicAliasAnalysis: public ModulePass, public AliasAnalysis, public LogProcessor {
  typedef DenseMap<std::pair<void *, unsigned>, std::vector<unsigned> >
      PointedByMapTy;
  typedef DenseMap<unsigned, std::pair<void *, unsigned> > PointsToMapTy;

  static char ID;
  static const unsigned UnknownVersion;

  DynamicAliasAnalysis(): ModulePass(ID) {
    CurrentVersion = 0;
  }
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  // Interfaces of AliasAnalysis.
  AliasAnalysis::AliasResult alias(const Location &L1, const Location &L2);
  virtual void *getAdjustedAnalysisPointer(AnalysisID PI);

  // Interfaces of LogProcessor.
  void processAddrTakenDecl(const AddrTakenDeclLogRecord &Record);
  void processTopLevelPointTo(const TopLevelPointToLogRecord &Record);
  void processAddrTakenPointTo(const AddrTakenPointToLogRecord &Record);

  const DenseSet<rcs::ValuePair> &getAllAliases() const {
    return Aliases;
  }

 private:
  // Returns the current version of <Addr>.
  unsigned lookupAddress(void *Addr) const;
  void removePointingTo(unsigned ValueID);
  void addPointingTo(unsigned ValueID, void *Address, unsigned Version);
  // A convenient wrapper for a batch of reports.
  void addAliasPairs(unsigned VID1, const std::vector<unsigned> &VID2s);
  // A convenient wrapper of addAliasPair(Value *, Value *).
  void addAliasPair(unsigned VID1, unsigned VID2);
  // Report <V1, V2> as an alias pair.
  // This function canonicalizes the pair, so that <V1, V2> and
  // <V2, V1> are considered the same.
  void addAliasPair(Value *V1, Value *V2);

  // An interval tree that maps addresses to version numbers.
  // We need store version numbers because pointing to the same address is
  // not enough to claim two pointers alias.
  std::map<Interval, unsigned> AddressVersion;
  unsigned CurrentVersion;
  // (address, version) => vid
  // <address> at version <version> is being pointed by <vid>.
  PointedByMapTy BeingPointedBy;
  // vid => (address, version)
  // Value <vid> is pointing to <address> at version <version>.
  PointsToMapTy PointingTo;
  // Stores all alias pairs.
  DenseSet<std::pair<Value *, Value *> > Aliases;
};
}

#endif
