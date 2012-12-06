// Author: Junyang

#define DEBUG_TYPE "dyn-aa"

#include <cstdio>
#include <fstream>
#include <algorithm>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/LogCounter.h"
#include "dyn-aa/MissingAliasesClassifier.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

static RegisterPass<MissingAliasesClassifier> X("classify-missing-aliases",
                                                "Classify All Missing Aliases",
                                                false, // Is CFG Only?
                                                true); // Is Analysis?

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

void MissingAliasesClassifier::processTopLevel(const TopLevelRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getValue(Record.PointerValueID);
  assert(V->getType()->isPointerTy());

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
  if (SelectPHIMem[Record.PointeeAddress].empty()) {
    SelectPHIMem.erase(Record.PointeeAddress);
  }

  // push to Mem
  if (Record.PointeeAddress != NULL) {
    CallSite CS(V);
    if (isa<LoadInst>(V)) {
      LoadMem[Record.LoadedFrom].push_back(V);
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
  for (vector<Value *>::iterator I = LoadMem[Record.PointerAddress].begin(),
       E = LoadMem[Record.PointerAddress].end(); I != E; ++I) {
    PrevInst[*I].insert(V);
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
      PrevInst[A].insert(CS.getArgument(A->getArgNo()));
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
  if (V1 > V2)
    swap(V1, V2);
  return binary_search(MissingAliases.begin(), MissingAliases.end(),
                       make_pair(V1, V2));
}

// when V2 is not LoadInst,
// return whether V1 is previous top level of V2
bool MissingAliasesClassifier::isDirectSrc(Value *V1, Value *V2) {
  CallSite CS2(V2);
  if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(V2)) {
    return V1 == GEPI->getPointerOperand();
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V2)) {
    return V1 == BCI->getOperand(0);
  } else if (LoadInst *LI = dyn_cast<LoadInst>(V2)) {
    for (set<Value *>::iterator I = PrevInst[V2].begin(),
         E = PrevInst[V2].end(); I != E; ++I) {
      StoreInst *SI = dyn_cast<StoreInst>(*I);
      if (V1 == SI->getValueOperand()) {
        // when V2 is LoadInst and V1 is previous top level of V2,
        // V1 is not direct source of V2,
        // if the pointers of StoreInst and LoadInst are missing alias
        return !isMissingAlias(LI->getPointerOperand(),
                               SI->getPointerOperand());
      }
    }
    return false;
  } else if (CS2 && PrevInst[V2].empty()) {
    // external function
    return false;
  } else if (isa<Argument>(V2) || CS2 || isa<PHINode>(V2) ||
             isa<SelectInst>(V2)) {
    for (set<Value *>::iterator I = PrevInst[V2].begin(),
         E = PrevInst[V2].end(); I != E; ++I) {
      if (V1 == *I) {
        return true;
      }
    }
    return false;
  } else if (isa<IntToPtrInst>(V2)) {
    // we can't handle IntToPtrInst, so conservatively return true
    return true;
  }
  // global value, alloca
  return false;
}

bool MissingAliasesClassifier::isRootCause(Value *V1, Value *V2) {
  // we consider three cases that a missing alias is root cause
  // case 1: one value is direct source of the other value
  if (isDirectSrc(V1, V2) || isDirectSrc(V2, V1)) {
    return true;
  }

  // case 2: both values are LoadInst, and their pointers are not missing alias
  LoadInst *LI1 = dyn_cast<LoadInst>(V1);
  LoadInst *LI2 = dyn_cast<LoadInst>(V2);
  if (LI1 && LI2) {
    return !isMissingAlias(LI1->getPointerOperand(), LI2->getPointerOperand());
  }

  // case 3: both values are GEPInst, and their sources are not missing alias
  GetElementPtrInst *GEPI1 = dyn_cast<GetElementPtrInst>(V1);
  GetElementPtrInst *GEPI2 = dyn_cast<GetElementPtrInst>(V2);
  if (GEPI1 && GEPI2) {
    return !isMissingAlias(GEPI1->getPointerOperand(),
                           GEPI2->getPointerOperand());
  }
  return false;
}
