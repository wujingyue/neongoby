// Author: Junyang

#define DEBUG_TYPE "dyn-aa"

#include <cstdio>
#include <fstream>
#include <algorithm>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Operator.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/LogCounter.h"
#include "dyn-aa/MissingAliasesClassifier.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

static RegisterPass<MissingAliasesClassifier> X("classify-missing-aliases",
                                                "Classify All Missing Aliases",
                                                false, // Is CFG Only?
                                                true); // Is Analysis?

STATISTIC(NumhasMAPointers, "Number of non-root missing aliases of pointers");
STATISTIC(NumIsMAWithOtherPredecessor,
          "Number of non-root missing aliases of other predecessor");
STATISTIC(NumIsMAWithSelfPredecessor,
          "Number of non-root missing aliases of self predecessor");
STATISTIC(NumLoadInst, "Number of non-root missing aliases of Load Inst");
STATISTIC(NumGEPInst, "Number of non-root missing aliases of GEP Inst");
STATISTIC(NumITPInst, "Number of root missing aliases of ITP Inst");
STATISTIC(NumRootCauses, "Number of root missing aliases");

char MissingAliasesClassifier::ID = 0;

bool MissingAliasesClassifier::runOnModule(Module &M) {
  errs() << "Backward processing...\n";
  processLog(true);

  return false;
}

void MissingAliasesClassifier::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
}

void MissingAliasesClassifier::setMissingAliases(const vector<ValuePair> &MA) {
  MissingAliases.insert(MissingAliases.begin(), MA.begin(), MA.end());
}

bool MissingAliasesClassifier::isRootCause(Value *V1, Value *V2) {
  // we consider five cases that a missing alias is not root cause
  // case 1: One value is LoadInst, and the other value is its predecessor,
  // and the pointer operands of LoadInst and StoreInst are missing alias.
  if (hasMissingAliasPointers(V1, V2) || hasMissingAliasPointers(V2, V1)) {
    NumhasMAPointers++;
    return false;
  }

  vector<Value *> Predecessors1, Predecessors2;
  getPredecessors(V1, Predecessors1);
  getPredecessors(V2, Predecessors2);
  // case 2: One value and predecessor of the other value are missing alias
  for (unsigned i = 0; i < Predecessors1.size(); ++i) {
    if (isMissingAlias(V2, Predecessors1[i])) {
      NumIsMAWithOtherPredecessor++;
      return false;
    }
  }
  for (unsigned i = 0; i < Predecessors2.size(); ++i) {
    if (isMissingAlias(V1, Predecessors2[i])) {
      NumIsMAWithOtherPredecessor++;
      return false;
    }
  }

  // case 3: One value and its predecessor are missing alias,
  // and the other value has no predecessor,
  // and the other value is not predecessor of the first value.
  if (Predecessors1.empty() && !isa<IntToPtrInst>(V1)) {
    unsigned i;
    for (i = 0; i < Predecessors2.size(); ++i) {
      if (V1 == Predecessors2[i]) {
        break;
      }
    }
    if (i == Predecessors2.size()) {
      for (i = 0; i < Predecessors2.size(); ++i) {
        if (isMissingAlias(V2, Predecessors2[i])) {
          NumIsMAWithSelfPredecessor++;
          return false;
        }
      }
    }
  }
  if (Predecessors2.empty() && !isa<IntToPtrInst>(V2)) {
    unsigned i;
    for (i = 0; i < Predecessors1.size(); ++i) {
      if (V2 == Predecessors1[i]) {
        break;
      }
    }
    if (i == Predecessors1.size()) {
      for (i = 0; i < Predecessors1.size(); ++i) {
        if (isMissingAlias(V1, Predecessors1[i])) {
          NumIsMAWithSelfPredecessor++;
          return false;
        }
      }
    }
  }

  // case 4: Both values are LoadInst, and their pointers are missing alias
  LoadInst *LI1 = dyn_cast<LoadInst>(V1);
  LoadInst *LI2 = dyn_cast<LoadInst>(V2);
  if (LI1 && LI2) {
    if (isMissingAlias(LI1->getPointerOperand(), LI2->getPointerOperand())) {
      NumLoadInst++;
      return false;
    }
  }

  // case 5: Both values are GEPInst, and their sources are missing alias
  GetElementPtrInst *GEPI1 = dyn_cast<GetElementPtrInst>(V1);
  GetElementPtrInst *GEPI2 = dyn_cast<GetElementPtrInst>(V2);
  if (GEPI1 && GEPI2) {
    if (isMissingAlias(GEPI1->getPointerOperand(),
                       GEPI2->getPointerOperand())) {
      NumGEPInst++;
      return false;
    }
  }

  // conservatively return true because we can't handle ITPInst
  if (isa<IntToPtrInst>(V1) || isa<IntToPtrInst>(V2)) {
    NumITPInst++;
  }

  NumRootCauses++;

  return true;
}

void MissingAliasesClassifier::processTopLevel(const TopLevelRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getValue(Record.PointerValueID);
  assert(V->getType()->isPointerTy());

  // extract from LoadMem
  if (isa<GlobalValue>(V)) {
    for (DenseMap<void *, list<pair<Value *, void *> > >::iterator DI =
         LoadMem.begin(), DE = LoadMem.end(); DI != DE; ++DI) {
      for (list<pair<Value *, void *> >::iterator VI = DI->second.begin(),
           VE = DI->second.end(); VI != VE;) {
        if (VI->second == Record.PointeeAddress) {
          PrevInst[VI->first].insert(V);
          VI = DI->second.erase(VI);
        } else {
          VI++;
        }
      }
    }
  }

  // extract from SelectPHIMem
  for (list<Value *>::iterator I = SelectPHIMem[Record.PointeeAddress].begin(),
       E = SelectPHIMem[Record.PointeeAddress].end(); I != E;) {
    bool found = false;
    if (SelectInst *SI = dyn_cast<SelectInst>(*I)) {
      if (V == SI->getTrueValue() || V == SI->getFalseValue()) {
        found = true;
      }
    } else if (PHINode *PN = dyn_cast<PHINode>(*I)) {
      unsigned NumIncomingValues = PN->getNumIncomingValues();
      for (unsigned i = 0; i != NumIncomingValues; ++i) {
        if (V == PN->getIncomingValue(i)) {
          found = true;
          break;
        }
      }
    }
    if (found) {
      PrevInst[*I].insert(V);
      I = SelectPHIMem[Record.PointeeAddress].erase(I);
    } else
      ++I;
  }

  // push to Mem
  if (Record.PointeeAddress != NULL) {
    CallSite CS(V);
    if (isa<LoadInst>(V)) {
      LoadMem[Record.LoadedFrom].push_back(make_pair(V, Record.PointeeAddress));
    } else if (CS) {
      CallMem.push_back(V);
    } else if (isa<Argument>(V)) {
      ArgMem.push_back(V);
    } else if (isa<SelectInst>(V) || isa<PHINode>(V)) {
      SelectPHIMem[Record.PointeeAddress].push_back(V);
    }
  }
}

void MissingAliasesClassifier::processStore(const StoreRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getInstruction(Record.InstructionID);
  assert(isa<StoreInst>(V));

  // extract from LoadMem
  for (list<pair<Value *, void *> >::iterator I =
       LoadMem[Record.PointerAddress].begin(),
       E = LoadMem[Record.PointerAddress].end(); I != E; ++I) {
    PrevInst[I->first].insert(V);
  }
  LoadMem.erase(Record.PointerAddress);
}

void MissingAliasesClassifier::processCall(const CallRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getInstruction(Record.InstructionID);
  CallSite CS(V);
  assert(CS);

  // extract from ArgMem
  for (list<Value *>::iterator I = ArgMem.begin(), E = ArgMem.end(); I != E;) {
    Argument *A = dyn_cast<Argument>(*I);
    if (TraceSlicer::isCalledFunction(A->getParent(), CS)) {
      PrevInst[A].insert(
          TraceSlicer::getOperandIfConstant(CS.getArgument(A->getArgNo())));
      I = ArgMem.erase(I);
    } else
      ++I;
  }

  // extract from CallMem, for external function
  if (CS.getType()->isPointerTy()) {
    for (list<Value *>::iterator I = CallMem.begin(), E = CallMem.end();
         I != E;) {
      if (V == *I) {
        I = CallMem.erase(I);
      } else
        ++I;
    }
  }
}

void MissingAliasesClassifier::processReturn(const ReturnRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getInstruction(Record.InstructionID);
  ReturnInst *RI = dyn_cast<ReturnInst>(V);
  // currently we could not deal with ResumeInst
  assert(RI);

  Value *ReturnValue = RI->getReturnValue();
  if (ReturnValue && ReturnValue->getType()->isPointerTy()) {
    // extract from CallMem
    for (list<Value *>::iterator I = CallMem.begin(), E = CallMem.end();
         I != E;) {
      CallSite CS(*I);
      if (TraceSlicer::isCalledFunction(RI->getParent()->getParent(), CS)) {
        PrevInst[*I].insert(ReturnValue);
        I = CallMem.erase(I);
      } else
        ++I;
    }
  }
}

bool MissingAliasesClassifier::isMissingAlias(Value *V1, Value *V2) {
  if (V1 == V2)
    return false;
  if (V1 > V2)
    swap(V1, V2);
  return binary_search(MissingAliases.begin(), MissingAliases.end(),
                       make_pair(V1, V2));
}

// whether V2 is LoadInst and V1 is predecessor of V2 from StoreInst,
// and pointers of StoreInst and LoadInst are missing alias
bool MissingAliasesClassifier::hasMissingAliasPointers(Value *V1, Value *V2) {
  if (LoadInst *LI = dyn_cast<LoadInst>(V2)) {
    for (set<Value *>::iterator I = PrevInst[V2].begin(),
         E = PrevInst[V2].end(); I != E; ++I) {
      if (StoreInst *SI = dyn_cast<StoreInst>(*I)) {
        if (V1 == TraceSlicer::getOperandIfConstant(SI->getValueOperand())) {
          if (isMissingAlias(LI->getPointerOperand(),
                             SI->getPointerOperand())) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void MissingAliasesClassifier::getPredecessors(Value *V,
                                               vector<Value *> &Predecessors) {
  CallSite CS(V);
  if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(V)) {
    Predecessors.push_back(GEPI->getPointerOperand());
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
    Predecessors.push_back(BCI->getOperand(0));
  } else if (isa<LoadInst>(V)) {
    for (set<Value *>::iterator I = PrevInst[V].begin(),
         E = PrevInst[V].end(); I != E; ++I) {
      if (StoreInst *SI = dyn_cast<StoreInst>(*I)) {
        Predecessors.push_back(
            TraceSlicer::getOperandIfConstant(SI->getValueOperand()));
      } else {
        Predecessors.push_back(*I);
      }
    }
  } else if (CS && PrevInst[V].empty()) {
    // external function
  } else if (isa<Argument>(V) || CS || isa<PHINode>(V) ||
             isa<SelectInst>(V)) {
    for (set<Value *>::iterator I = PrevInst[V].begin(),
         E = PrevInst[V].end(); I != E; ++I) {
      Predecessors.push_back(*I);
    }
  }
  // global value, MallocInst and unhandled instructions have no predecessors
}
