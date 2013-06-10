// Author: Junyang

#ifndef __DYN_AA_MISSING_ALIASES_CLASSIFIER_H
#define __DYN_AA_MISSING_ALIASES_CLASSIFIER_H

#include <utility>
#include <vector>
#include <set>
#include <list>

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/ADT/DenseMap.h"

#include "rcs/typedefs.h"

#include "dyn-aa/TraceSlicer.h"
#include "dyn-aa/LogRecord.h"
#include "dyn-aa/LogProcessor.h"
#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;
using namespace rcs;

namespace neongoby {
struct MissingAliasesClassifier: public ModulePass, public LogProcessor {
  static char ID;

  MissingAliasesClassifier(): ModulePass(ID) {}
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  void setMissingAliases(const vector<ValuePair> &MA);
  bool isRootCause(Value *V1, Value *V2);

  // Interfaces of LogProcessor.
  void processTopLevel(const TopLevelRecord &Record);
  void processStore(const StoreRecord &Record);
  void processCall(const CallRecord &Record);
  void processReturn(const ReturnRecord &Record);

 private:
  vector<ValuePair> MissingAliases;
  // Previous instruction for all pointer type values
  DenseMap<Value *, set<Value *> > PrevInst;
  // Argument list
  list<Value *> ArgMem;
  // CallSite list
  list<Value *> CallMem;
  // Keys are PointerAddress, values are list of <LoadInst, PointeeAddress>
  DenseMap<void *, list<pair<Value *, void *> > > LoadMem;
  // Keys are PointeeAddress, values are SelectInst or PHINode list
  DenseMap<void *, list<Value *> > SelectPHIMem;
  bool isMissingAlias(Value *V1, Value *V2);
  bool hasMissingAliasPointers(Value *V1, Value *V2);
  void getPredecessors(Value *V, vector<Value *> &Predecessors);
};
}

#endif
