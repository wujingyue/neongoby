#define DEBUG_TYPE "dyn-aa"

#include <cstdio>
#include <fstream>

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/LogCounter.h"
#include "dyn-aa/TraceSlicer.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

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

  void processTopLevelPointTo(const TopLevelPointToLogRecord &Record) {
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
                             pair<unsigned, unsigned> TraceRecord,
                             int PointerLabel) const {
  unsigned RecordID = TraceRecord.first;
  unsigned ValueID = TraceRecord.second;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getValue(ValueID);
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
    unsigned Min;
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

void TraceSlicer::processAddrTakenDecl(const AddrTakenDeclLogRecord &Record) {
  CurrentRecordID--;
  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
  }
}

void TraceSlicer::processTopLevelPointTo(
    const TopLevelPointToLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  LogRecordInfo CurrentRecord;
  CurrentRecord.ValueID = Record.PointerValueID;
  CurrentRecord.PointeeAddress = Record.PointeeAddress;
  CurrentRecord.PointerAddress = Record.LoadedFrom;

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    if (Trace[PointerLabel].StartingRecordID == CurrentRecordID) {
      Value *V = IDA.getValue(Record.PointerValueID);

      // input value must be a pointer
      assert(V->getType()->isPointerTy());

      // set StartingFunction
      if (Argument *A = dyn_cast<Argument>(V))
        Trace[PointerLabel].StartingFunction = A->getParent();
      else if (Instruction *I = dyn_cast<Instruction>(V))
        Trace[PointerLabel].StartingFunction = I->getParent()->getParent();
      else
        Trace[PointerLabel].StartingFunction = NULL;

      Trace[PointerLabel].Active = true;
      Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                    CurrentRecord.ValueID));
      Trace[PointerLabel].PreviousRecord = CurrentRecord;
      NumContainingSlices++;
    } else {
      if (dependsOn(Trace[PointerLabel], CurrentRecord, IDA)) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.ValueID));
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

void TraceSlicer::processAddrTakenPointTo(
    const AddrTakenPointToLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  assert(isa<StoreInst>(I));

  LogRecordInfo CurrentRecord;
  CurrentRecord.ValueID = IDA.getValueID(I);
  CurrentRecord.PointerAddress = Record.PointerAddress;

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (dependsOn(Trace[PointerLabel], CurrentRecord, IDA)) {
      Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                    CurrentRecord.ValueID));
      Trace[PointerLabel].PreviousRecord = CurrentRecord;
      NumContainingSlices++;
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    Trace[0].Active = false;
    Trace[1].Active = false;
  }
}

void TraceSlicer::processCallInstruction(
    const CallInstructionLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  CallSite CS(I);
  assert(CS);

  LogRecordInfo CurrentRecord;
  CurrentRecord.ValueID = IDA.getValueID(I);

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (dependsOn(Trace[PointerLabel], CurrentRecord, IDA)) {
      Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                    CurrentRecord.ValueID));
      Trace[PointerLabel].PreviousRecord = CurrentRecord;
      NumContainingSlices++;
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    Trace[0].Active = false;
    Trace[1].Active = false;
  }
}

void TraceSlicer::processReturnInstruction(
    const ReturnInstructionLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  assert(isa<ReturnInst>(I) || isa<ResumeInst>(I));

  LogRecordInfo CurrentRecord;
  CurrentRecord.ValueID = IDA.getValueID(I);

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (dependsOn(Trace[PointerLabel], CurrentRecord, IDA)) {
      Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                    CurrentRecord.ValueID));
      Trace[PointerLabel].PreviousRecord = CurrentRecord;
      NumContainingSlices++;
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
      // print return instruction of the starting function
      if (RI->getParent()->getParent() == Trace[PointerLabel].StartingFunction) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.ValueID));
      }
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    Trace[0].Active = false;
    Trace[1].Active = false;
  }
}

// whether trace depend on current record
bool TraceSlicer::dependsOn(PointerTrace &Trace,
                            LogRecordInfo &CurrentRecord,
                            rcs::IDAssigner &IDA) {
  if (!Trace.Active) {
    return false;
  }

  LogRecordInfo &PreviousRecord = Trace.PreviousRecord;
  Value *CurrentValue = IDA.getValue(CurrentRecord.ValueID);
  Value *PreviousValue = IDA.getValue(PreviousRecord.ValueID);
  // currently we could not deal with ResumeInst
  assert(!isa<ResumeInst>(CurrentValue));
  CallSite PreviousCS(PreviousValue);
  CallSite CurrentCS(CurrentValue);

  if (PreviousCS) {
    if (CurrentRecord.ValueID == PreviousRecord.ValueID) {
      // Can't find ReturnInst in poiter log,
      // PreviousRecord is an external function call
      Trace.Active = false;
      return false;
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(CurrentValue)) {
      if (RI->getParent()->getParent() == PreviousCS.getCalledFunction()) {
        return true;
      }
      return CurrentRecord.ValueID ==
             IDA.getValueID(PreviousCS.getArgument(PreviousRecord.ArgNo));
    } else {
      return CurrentRecord.ValueID ==
             IDA.getValueID(PreviousCS.getArgument(PreviousRecord.ArgNo));
    }
  } else if (Argument *A = dyn_cast<Argument>(PreviousValue)) {
    if (CurrentCS) {
      CurrentRecord.ArgNo = A->getArgNo();
      return true;
    }
    return false;
  } else if (GetElementPtrInst *GEPI =
             dyn_cast<GetElementPtrInst>(PreviousValue)) {
    return CurrentRecord.ValueID == IDA.getValueID(GEPI->getPointerOperand());
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(PreviousValue)) {
    return CurrentRecord.ValueID == IDA.getValueID(BCI->getOperand(0));
  } else if (StoreInst *SI = dyn_cast<StoreInst>(PreviousValue)) {
    return CurrentRecord.ValueID == IDA.getValueID(SI->getValueOperand());
  } else if (ReturnInst *RI = dyn_cast<ReturnInst>(PreviousValue)) {
    return CurrentRecord.ValueID == IDA.getValueID(RI->getReturnValue());
  } else if (SelectInst *SI = dyn_cast<SelectInst>(PreviousValue)) {
    return CurrentRecord.PointeeAddress == PreviousRecord.PointeeAddress &&
    (CurrentRecord.ValueID == IDA.getValueID(SI->getTrueValue()) ||
     CurrentRecord.ValueID == IDA.getValueID(SI->getTrueValue()));
  } else if (PHINode *PN = dyn_cast<PHINode>(PreviousValue)) {
    if (CurrentRecord.PointeeAddress == PreviousRecord.PointeeAddress) {
      unsigned NumIncomingValues = PN->getNumIncomingValues();
      for (unsigned VI = 0; VI != NumIncomingValues; ++VI) {
        if (CurrentRecord.ValueID == IDA.getValueID(PN->getIncomingValue(VI))) {
          return true;
        }
      }
    }
    return false;
  } else if (isa<LoadInst>(PreviousValue)) {
    return isa<StoreInst>(CurrentValue) &&
           CurrentRecord.PointerAddress == PreviousRecord.PointerAddress;
  } else {
    // PreviousRecord may be Global Variable, MallocInst,
    // or unsupported Instructions
    Trace.Active = false;
    return false;
  }
}

// Get the latest common ancestor of the two slices.
// Returns NULL if the two slices don't meet.
Value *TraceSlicer::getLatestCommonAncestor() {
  if (Trace[0].Slice.empty() || Trace[1].Slice.empty())
    return NULL;
  if (Trace[0].Slice.back() == Trace[1].Slice.back()) {
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    return IDA.getValue(Trace[0].Slice.back().second);
  }
  return NULL;
}
