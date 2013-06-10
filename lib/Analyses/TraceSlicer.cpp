// Author: Junyang

#define DEBUG_TYPE "dyn-aa"

#include <cstdio>
#include <fstream>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Operator.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/LogCounter.h"
#include "dyn-aa/TraceSlicer.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

// Users specify either StartingRecordIDs or StartingValueIDs.
static cl::list<unsigned> StartingRecordIDs(
    "starting-record",
    cl::desc("Record IDs of the two pointers"));
// When users specify StartingValueIDs, we look for the first two records of the
// given values that share the same address. Note that sharing the same address
// does not always mean they alias because of versioning issues and such. But
// most of the time it just works. Therefore, be careful about this option.
static cl::list<unsigned> StartingValueIDs(
    "starting-value",
    cl::desc("Value IDs of the two pointers"));

static RegisterPass<TraceSlicer> X("slice-trace",
                                   "Slice trace of two input pointers",
                                   false, // Is CFG Only?
                                   true); // Is Analysis?

struct RecordFinder: public LogProcessor {
  RecordFinder(): RecordID1(-1), RecordID2(-1) {}

  void processTopLevel(const TopLevelRecord &Record) {
    if (StartingRecordIDs.size() == 2) {
      // Do nothing if already filled in.
      return;
    }
    if (Record.PointerValueID == StartingValueIDs[0]) {
      RecordID1 = getCurrentRecordID();
      Address1 = Record.PointeeAddress;
    }
    if (Record.PointerValueID == StartingValueIDs[1]) {
      RecordID2 = getCurrentRecordID();
      Address2 = Record.PointeeAddress;
    }
    if (Address1 == Address2) {
      StartingRecordIDs.push_back(RecordID1);
      StartingRecordIDs.push_back(RecordID2);
      assert(StartingRecordIDs.size() == 2);
    }
  }

 private:
  unsigned RecordID1, RecordID2;
  void *Address1, *Address2;
};

char TraceSlicer::ID = 0;

bool TraceSlicer::runOnModule(Module &M) {
  assert((StartingRecordIDs.empty() ^ StartingValueIDs.empty()) &&
         "specify either starting-record or starting-value");
  assert((StartingRecordIDs.empty() || StartingRecordIDs.size() == 2) &&
         "we need two starting-record");
  assert((StartingValueIDs.empty() || StartingValueIDs.size() == 2) &&
         "we need two starting-value");
  if (StartingRecordIDs.empty()) {
    // The user specifies staring-value instead of starting-record. Need look
    // for starting-record in the trace.
    errs() << "Finding records of the two input values...\n";
    RecordFinder RF;
    RF.processLog();
    CurrentRecordID = RF.getCurrentRecordID();
  } else {
    errs() << "Counting log records...\n";
    LogCounter LC;
    LC.processLog();
    CurrentRecordID = LC.getNumLogRecords();
  }

  assert(StartingRecordIDs.size() == 2);
  for (unsigned i = 0; i < StartingRecordIDs.size(); ++i)
    Trace[i].StartingRecordID = StartingRecordIDs[i];

  errs() << "Backward slicing...\n";
  processLog(true);

  return false;
}

void TraceSlicer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
}

void TraceSlicer::printTrace(raw_ostream &O,
                             pair<unsigned, Value *> TraceRecord,
                             int PointerLabel) const {
  unsigned RecordID = TraceRecord.first;
  Value *V = TraceRecord.second;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  unsigned ValueID = IDA.getValueID(V);
  O.changeColor(PointerLabel == 0 ? raw_ostream::GREEN : raw_ostream::RED);
  O << RecordID << "\t";
  O << "ptr" << PointerLabel + 1 << "\t";
  O << ValueID << "\t";
  DynAAUtils::PrintValue(O, V);
  O << "\n";
  O.resetColor();
}

void TraceSlicer::print(raw_ostream &O, const Module *M) const {
  O << "RecID\tPtr\tValueID\tFunc:  Inst/Arg\n";
  int Index[2];
  Index[0] = Trace[0].Slice.size() - 1;
  Index[1] = Trace[1].Slice.size() - 1;
  while (true) {
    unsigned Min = UINT_MAX;
    int PointerLabel = -1;
    for (int i = 0; i < 2; ++i) {
      if (Index[i] >= 0 &&
          (PointerLabel == -1 || Trace[i].Slice[Index[i]].first < Min)) {
        Min = Trace[i].Slice[Index[i]].first;
        PointerLabel = i;
      }
    }
    if (PointerLabel != -1) {
      printTrace(O,
                 Trace[PointerLabel].Slice[Index[PointerLabel]],
                 PointerLabel);
      Index[PointerLabel]--;
    } else
      break;
  }
}

void TraceSlicer::processMemAlloc(const MemAllocRecord &Record) {
  CurrentRecordID--;
  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
  }
}

void TraceSlicer::processTopLevel(const TopLevelRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getValue(Record.PointerValueID);
  // input value must be a pointer
  assert(V->getType()->isPointerTy());

  LogRecordInfo CurrentRecord;
  CurrentRecord.V = V;
  CurrentRecord.PointeeAddress = Record.PointeeAddress;
  CurrentRecord.PointerAddress = Record.LoadedFrom;

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    if (Trace[PointerLabel].StartingRecordID == CurrentRecordID) {
      // set StartingFunction
      if (Argument *A = dyn_cast<Argument>(V))
        Trace[PointerLabel].StartingFunction = A->getParent();
      else if (Instruction *I = dyn_cast<Instruction>(V))
        Trace[PointerLabel].StartingFunction = I->getParent()->getParent();
      else
        Trace[PointerLabel].StartingFunction = NULL;

      Trace[PointerLabel].Active = true;
      Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                    CurrentRecord.V));
      Trace[PointerLabel].PreviousRecord = CurrentRecord;
      NumContainingSlices++;
    } else if (Trace[PointerLabel].Active) {
      pair<bool, bool> Result = dependsOn(CurrentRecord,
                                          Trace[PointerLabel].PreviousRecord);
      Trace[PointerLabel].Active = Result.second;
      if (Result.first) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.V));
        Trace[PointerLabel].PreviousRecord = CurrentRecord;
        NumContainingSlices++;
      }
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    Trace[0].Active = false;
    Trace[1].Active = false;
  }
}

void TraceSlicer::processStore(const StoreRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  assert(isa<StoreInst>(I));

  LogRecordInfo CurrentRecord;
  CurrentRecord.V = I;
  CurrentRecord.PointerAddress = Record.PointerAddress;

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (Trace[PointerLabel].Active) {
      pair<bool, bool> Result = dependsOn(CurrentRecord,
                                          Trace[PointerLabel].PreviousRecord);
      Trace[PointerLabel].Active = Result.second;
      if (Result.first) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.V));
        Trace[PointerLabel].PreviousRecord = CurrentRecord;
        NumContainingSlices++;
      }
    }
  }
}

void TraceSlicer::processCall(const CallRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  CallSite CS(I);
  assert(CS);

  LogRecordInfo CurrentRecord;
  CurrentRecord.V = I;

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (Trace[PointerLabel].Active) {
      pair<bool, bool> Result = dependsOn(CurrentRecord,
                                          Trace[PointerLabel].PreviousRecord);
      Trace[PointerLabel].Active = Result.second;
      if (Result.first) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.V));
        Trace[PointerLabel].PreviousRecord = CurrentRecord;
        NumContainingSlices++;
      }
    }
  }
}

void TraceSlicer::processReturn(const ReturnRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  // currently we could not deal with ResumeInst
  assert(isa<ReturnInst>(I));

  LogRecordInfo CurrentRecord;
  CurrentRecord.V = I;

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (Trace[PointerLabel].Active) {
      pair<bool, bool> Result = dependsOn(CurrentRecord,
                                          Trace[PointerLabel].PreviousRecord);
      Trace[PointerLabel].Active = Result.second;
      if (Result.first) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.V));
        Trace[PointerLabel].PreviousRecord = CurrentRecord;
        NumContainingSlices++;
      } else {
        // print return instruction of the starting function
        if (I->getParent()->getParent() ==
            Trace[PointerLabel].StartingFunction) {
          Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                        CurrentRecord.V));
        }
      }
    }
  }
}

void TraceSlicer::processBasicBlock(const BasicBlockRecord &Record) {
  CurrentRecordID--;
  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
  }
}

// whether R1 depend on R2, return <depend, active>
pair<bool, bool> TraceSlicer::dependsOn(LogRecordInfo &R1, LogRecordInfo &R2) {
  CallSite CS1(R1.V);
  CallSite CS2(R2.V);

  if (CS2) {
    if (R2.ArgNo != -1) {
      // R2 is CallRecord
      return make_pair(R1.V == getOperandIfConstant(CS2.getArgument(R2.ArgNo)),
                       true);
    } else if (CS1 && R1.V == R2.V) {
      // R2 is an external function call
      return make_pair(false, false);
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(R1.V)) {
      return make_pair(isCalledFunction(RI->getParent()->getParent(), CS2),
                       true);
    } else {
      return make_pair(false, true);
    }
  } else if (Argument *A = dyn_cast<Argument>(R2.V)) {
    if (CS1 && isCalledFunction(A->getParent(), CS1)) {
      R1.ArgNo = A->getArgNo();
      return make_pair(true, true);
    } else {
      return make_pair(false, true);
    }
  } else if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(R2.V)) {
    return make_pair(R1.V == GEPI->getPointerOperand(), true);
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(R2.V)) {
    return make_pair(R1.PointeeAddress == R2.PointeeAddress &&
                     R1.V == BCI->getOperand(0), true);
  } else if (StoreInst *SI = dyn_cast<StoreInst>(R2.V)) {
    return make_pair(R1.V == getOperandIfConstant(SI->getValueOperand()), true);
  } else if (ReturnInst *RI = dyn_cast<ReturnInst>(R2.V)) {
    return make_pair(R1.V == RI->getReturnValue(), true);
  } else if (SelectInst *SI = dyn_cast<SelectInst>(R2.V)) {
    return make_pair(R1.PointeeAddress == R2.PointeeAddress &&
                     (R1.V == SI->getTrueValue() ||
                      R1.V == SI->getFalseValue()), true);
  } else if (PHINode *PN = dyn_cast<PHINode>(R2.V)) {
    if (R1.PointeeAddress == R2.PointeeAddress) {
      unsigned NumIncomingValues = PN->getNumIncomingValues();
      for (unsigned VI = 0; VI != NumIncomingValues; ++VI) {
        if (R1.V == PN->getIncomingValue(VI)) {
          return make_pair(true, true);
        }
      }
    }
    return make_pair(false, true);
  } else if (isa<LoadInst>(R2.V)) {
    if (isa<StoreInst>(R1.V)) {
      return make_pair(R1.PointerAddress == R2.PointerAddress, true);
    } else if (isa<GlobalValue>(R1.V)) {
      // predecessor of LoadInst may be a global value with same pointee address
      return make_pair(R1.PointeeAddress == R2.PointeeAddress, true);
    } else {
      return make_pair(false, true);
    }
  } else {
    // R2 may be Global Variable, MallocInst,
    // or unsupported Instructions
    return make_pair(false, false);
  }
}

bool TraceSlicer::isCalledFunction(Function *F, CallSite CS) {
  if (CS.getCalledFunction() != NULL)
    return F == CS.getCalledFunction();
  // if CS call a value, judge by comparing return type and argument type
  // this is a temporary method to solve multithread problem
  if (F->getReturnType() != CS.getType())
    return false;
  if (F->getFunctionType()->getNumParams() != CS.arg_size())
    return false;
  for (unsigned i = 0; i < CS.arg_size(); ++i)
    if (F->getFunctionType()->getParamType(i) != (CS.getArgument(i))->getType())
      return false;
  return true;
}

// get operand if V is a constant expression
Value *TraceSlicer::getOperandIfConstant(Value *V) {
  Operator *Op = dyn_cast<Operator>(V);
  if (isa<ConstantExpr>(V) && Op)
    return Op->getOperand(0);
  return V;
}

// Get the latest common ancestor of the two slices.
// Returns NULL if the two slices don't meet.
Value *TraceSlicer::getLatestCommonAncestor() {
  if (Trace[0].Slice.empty() || Trace[1].Slice.empty())
    return NULL;
  if (Trace[0].Slice.back() == Trace[1].Slice.back()) {
    return Trace[0].Slice.back().second;
  }
  return NULL;
}
