#define DEBUG_TYPE "dyn-aa"

#include <cstdio>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"

#include "dyn-aa/DynamicPointerAnalysis.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

static RegisterPass<DynamicPointerAnalysis> X("dyn-pa",
                                              "Build the point-to graph "
                                              "from the point-to log",
                                              false, // Is CFG Only?
                                              true); // Is Analysis?
static RegisterAnalysisGroup<PointerAnalysis> Y(X);

char DynamicPointerAnalysis::ID = 0;

bool DynamicPointerAnalysis::runOnModule(Module &M) {
  processLog();
  return false;
}

void DynamicPointerAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
}

void DynamicPointerAnalysis::processMemAlloc(const MemAllocRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  Value *Allocator = NULL;
  if (Record.AllocatedBy != IDAssigner::InvalidID)
    Allocator = IDA.getValue(Record.AllocatedBy);
  // Allocator may be NULL.
  // In that case, the memory block is allocated by an external instruction.
  // e.g. main arguments.

  unsigned long Start = (unsigned long)Record.Address;
  Interval I(Start, Start + Record.Bound);
  pair<IntervalTree<Value *>::iterator, IntervalTree<Value *>::iterator> ER =
      MemAllocs.equal_range(I);
  MemAllocs.erase(ER.first, ER.second);
  MemAllocs.insert(make_pair(I, Allocator));
}

void DynamicPointerAnalysis::processTopLevel(const TopLevelRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  Value *Pointer = IDA.getValue(Record.PointerValueID);
  Value *Pointee = lookupAddress(Record.PointeeAddress);
  assert(Pointer);
  if (Pointee) {
    PointTos[Pointer].insert(Pointee);
  }
}

Value *DynamicPointerAnalysis::lookupAddress(void *Addr) const {
  Interval I((unsigned long)Addr, (unsigned long)Addr + 1);
  IntervalTree<Value *>::const_iterator Pos = MemAllocs.find(I);
  if (Pos == MemAllocs.end())
    return NULL;
  return Pos->second;
}

bool DynamicPointerAnalysis::getPointees(const Value *Pointer,
                                         ValueList &Pointees) {
  Pointees.clear();
  DenseMap<const Value *, ValueSet>::iterator I = PointTos.find(Pointer);
  if (I == PointTos.end())
    return false;

  Pointees.insert(Pointees.end(), I->second.begin(), I->second.end());
  return true;
}

void DynamicPointerAnalysis::getAllPointers(ValueList &Pointers) {
  for (DenseMap<const Value *, ValueSet>::iterator I = PointTos.begin();
       I != PointTos.end(); ++I) {
    Pointers.push_back(const_cast<Value *>(I->first));
  }
}

void *DynamicPointerAnalysis::getAdjustedAnalysisPointer(AnalysisID PI) {
  if (PI == &PointerAnalysis::ID)
    return (PointerAnalysis *)this;
  return this;
}
