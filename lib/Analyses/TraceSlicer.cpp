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
    CurrentState[i].StartingRecordID = StartingRecordIDs[i];

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
  Index[0] = CurrentState[0].Trace.size() - 1;
  Index[1] = CurrentState[1].Trace.size() - 1;
  while (true) {
    unsigned Min;
    int PointerLabel = -1;
    for (int i = 0; i < 2; ++i) {
      if (Index[i] >= 0 &&
          (PointerLabel == -1 || CurrentState[i].Trace[Index[i]].first < Min)) {
        Min = CurrentState[i].Trace[Index[i]].first;
        PointerLabel = i;
      }
    }
    if (PointerLabel != -1) {
      printTrace(O,
                 CurrentState[PointerLabel].Trace[Index[PointerLabel]],
                 PointerLabel);
      Index[PointerLabel]--;
    } else
      break;
  }
}

bool TraceSlicer::isLive(int PointerLabel) {
  if (CurrentState[PointerLabel].StartingRecordID < CurrentRecordID ||
      CurrentState[PointerLabel].End) {
    return false;
  }
  return true;
}

void TraceSlicer::processAddrTakenDecl(const AddrTakenDeclLogRecord &Record) {
  CurrentRecordID--;
  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(CurrentState[PointerLabel].StartingRecordID != CurrentRecordID);
  }
}

void TraceSlicer::processTopLevelPointTo(
    const TopLevelPointToLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    if (CurrentState[PointerLabel].StartingRecordID == CurrentRecordID) {
      Value *V = IDA.getValue(Record.PointerValueID);
      assert(V->getType()->isPointerTy());
      if (Argument *A = dyn_cast<Argument>(V))
        CurrentState[PointerLabel].StartingFunction = A->getParent();
      else if (Instruction *I = dyn_cast<Instruction>(V))
        CurrentState[PointerLabel].StartingFunction =
          I->getParent()->getParent();
      else
        CurrentState[PointerLabel].StartingFunction = NULL;
      CurrentState[PointerLabel].ValueID = Record.PointerValueID;
    }
    if (isLive(PointerLabel)) {
      if (CurrentState[PointerLabel].Action != TopLevelPointTo) {
        continue;
      }

      if (!CurrentState[PointerLabel].ValueIDCandidates.empty()) {
        // For select and PHI, find current ID according to Address and
        // ValueIDCandidates.
        // If two variables of select have the same value, we follow the one
        // occurs latter, this is just a temporary method.
        if (Record.PointeeAddress == CurrentState[PointerLabel].Address &&
            CurrentState[PointerLabel].ValueIDCandidates.count(
              Record.PointerValueID)) {
          CurrentState[PointerLabel].ValueIDCandidates.clear();
          CurrentState[PointerLabel].ValueID = Record.PointerValueID;

          NumContainingSlices++;
          CurrentState[PointerLabel].Trace.push_back(
              make_pair(CurrentRecordID,
                        CurrentState[PointerLabel].ValueID));
          trackSourcePointer(CurrentState[PointerLabel], Record);
        }
      } else {
        if (Record.PointerValueID == CurrentState[PointerLabel].ValueID) {
          NumContainingSlices++;
          CurrentState[PointerLabel].Trace.push_back(
              make_pair(CurrentRecordID,
                        CurrentState[PointerLabel].ValueID));
          trackSourcePointer(CurrentState[PointerLabel], Record);
        }
      }
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    CurrentState[0].End = true;
    CurrentState[1].End = true;
  }
}

void TraceSlicer::trackSourcePointer(TraceState &TS,
                                    const TopLevelPointToLogRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Value *V = IDA.getValue(TS.ValueID);
  if (isa<LoadInst>(V)) {
    TS.Address = Record.LoadedFrom;
    TS.Action = AddrTakenPointTo;
  } else if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(V)) {
    TS.ValueID = IDA.getValueID(GEPI->getPointerOperand());
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
    TS.ValueID = IDA.getValueID(BCI->getOperand(0));
  } else if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
    TS.ValueIDCandidates.insert(IDA.getValueID(SI->getTrueValue()));
    TS.ValueIDCandidates.insert(IDA.getValueID(SI->getFalseValue()));
    TS.Address = Record.PointeeAddress;
  } else if (PHINode *PN = dyn_cast<PHINode>(V)) {
    unsigned NumIncomingValues = PN->getNumIncomingValues();
    for (unsigned VI = 0; VI != NumIncomingValues; ++VI) {
      TS.ValueIDCandidates.insert(IDA.getValueID(PN->getIncomingValue(VI)));
    }
    TS.Address = Record.PointeeAddress;
  } else if (Argument *A = dyn_cast<Argument>(V)) {
    TS.Action = CallInstruction;
    TS.ArgNo = A->getArgNo();
    // errs() << "(argument of @" << A->getParent()->getName() << ")\n";
  } else if (isa<CallInst>(V) || isa<InvokeInst>(V)) {
    TS.Action = ReturnInstruction;
  } else if (isa<AllocaInst>(V)) {
    TS.End = true;
  } else if (isa<GlobalValue>(V)) {
    TS.End = true;
  } else {
    errs() << "Unknown instruction \'" << *V << "\'\n";
    TS.End = true;
  }
}

void TraceSlicer::processAddrTakenPointTo(
    const AddrTakenPointToLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  assert(isa<StoreInst>(I));

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(CurrentState[PointerLabel].StartingRecordID != CurrentRecordID);
    if (isLive(PointerLabel)) {
      if (CurrentState[PointerLabel].Action != AddrTakenPointTo) {
        continue;
      }
      if (Record.PointerAddress == CurrentState[PointerLabel].Address) {
        NumContainingSlices++;
        CurrentState[PointerLabel].Trace.push_back(
            make_pair(CurrentRecordID,
                      IDA.getValueID(I)));
        CurrentState[PointerLabel].Action = TopLevelPointTo;
        StoreInst *SI = dyn_cast<StoreInst>(I);
        CurrentState[PointerLabel].ValueID =
          IDA.getValueID(SI->getValueOperand());
      }
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    CurrentState[0].End = true;
    CurrentState[1].End = true;
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

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(CurrentState[PointerLabel].StartingRecordID != CurrentRecordID);
    if (isLive(PointerLabel)) {
      if (CurrentState[PointerLabel].Action == ReturnInstruction) {
        // this callee is an external function, the trace ends
        CurrentState[PointerLabel].End = true;
        continue;
      }
      if (CurrentState[PointerLabel].Action != CallInstruction) {
        continue;
      }

      NumContainingSlices++;
      CurrentState[PointerLabel].Trace.push_back(
          make_pair(CurrentRecordID,
                    IDA.getValueID(I)));
      CurrentState[PointerLabel].Action = TopLevelPointTo;
      CurrentState[PointerLabel].ValueID =
        IDA.getValueID(CS.getArgument(CurrentState[PointerLabel].ArgNo));
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    CurrentState[0].End = true;
    CurrentState[1].End = true;
  }
}

void TraceSlicer::processReturnInstruction(
    const ReturnInstructionLogRecord &Record) {
  CurrentRecordID--;
  int NumContainingSlices = 0;
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  Instruction *I = IDA.getInstruction(Record.InstructionID);
  assert(isa<ReturnInst>(I) || isa<ResumeInst>(I));

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevelPointTo record
    assert(CurrentState[PointerLabel].StartingRecordID != CurrentRecordID);
    if (isLive(PointerLabel)) {
      if (CurrentState[PointerLabel].Action != ReturnInstruction) {
        // print return instruction of the starting function
        if (I->getParent()->getParent() ==
            CurrentState[PointerLabel].StartingFunction) {
          CurrentState[PointerLabel].Trace.push_back(
              make_pair(CurrentRecordID,
                        IDA.getValueID(I)));
        }
        continue;
      }

      NumContainingSlices++;
      CurrentState[PointerLabel].Trace.push_back(
          make_pair(CurrentRecordID,
                    IDA.getValueID(I)));
      CurrentState[PointerLabel].Action = TopLevelPointTo;
      if (ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
        CurrentState[PointerLabel].ValueID =
          IDA.getValueID(RI->getReturnValue());
      } else if (isa<ResumeInst>(I)) {
        assert(false);
      } else {
        assert(false);
      }
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    CurrentState[0].End = true;
    CurrentState[1].End = true;
  }
}

// Get the latest common ancestor of the two slices.
// Returns NULL if the two slices don't meet.
Value *TraceSlicer::getLatestCommonAncestor() {
  if (CurrentState[0].Trace.empty() || CurrentState[1].Trace.empty())
    return NULL;
  if (CurrentState[0].Trace.back() == CurrentState[1].Trace.back()) {
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    return IDA.getValue(CurrentState[0].Trace.back().second);
  }
  return NULL;
}
