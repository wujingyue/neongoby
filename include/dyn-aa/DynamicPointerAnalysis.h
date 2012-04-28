// Author: Jingyue
//
// A very precise but unsound pointer analysis. 
// The point-to relations are collected from a dynamic trace. 

#ifndef __DYN_AA_DYNAMIC_POINTER_ANALYSIS_H
#define __DYN_AA_DYNAMIC_POINTER_ANALYSIS_H

#include "llvm/Pass.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include "common/PointerAnalysis.h"
using namespace rcs;

#include "dyn-aa/LogRecord.h"
#include "dyn-aa/IntervalTree.h"
using namespace dyn_aa;

namespace dyn_aa {
struct DynamicPointerAnalysis: public ModulePass, public PointerAnalysis {
  static char ID;

  DynamicPointerAnalysis(): ModulePass(ID) {}
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  virtual void getAllPointers(ValueList &Pointers);
  virtual bool getPointees(const Value *Pointer, ValueList &Pointees);

  virtual void *getAdjustedAnalysisPointer(AnalysisID PI);

 private:
  void processAddrTakenDecl(const AddrTakenDeclLogRecord &Record);
  void processTopLevelPointTo(const TopLevelPointToLogRecord &Record);
  void processAddrTakenPointTo(const AddrTakenPointToLogRecord &Record);
  // Returns the value ID of <Addr>'s allocator. 
  // Possible allocators include malloc function calls, AllocaInsts, and
  // global variables. 
  Value *lookupAddress(void *Addr) const;

  // Stores all addr-taken declarations. 
  IntervalTree AddrTakenDecls;
  // Use DenseSet instead of vector, because they are usually lots of 
  // duplicated edges. 
  DenseMap<const Value *, ValueSet> PointTos;
};
}

#endif
