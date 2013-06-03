// Author: Junyang

#define DEBUG_TYPE "dyn-aa"

#include <cstdio>
#include <fstream>

#include "llvm/IntrinsicInst.h"
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

static cl::opt<bool> SliceForReduction("slice-for-reduction",
                                       cl::desc("Slice for reduction"));

static RegisterPass<TraceSlicer> X("slice-trace",
                                   "Slice trace of two input pointers",
                                   false, // Is CFG Only?
                                   true); // Is Analysis?

struct RecordFinder: public LogProcessor {
  RecordFinder() {}

  void processTopLevel(const TopLevelRecord &Record) {
    if (StartingRecordIDs.size() == 2) {
      // Do nothing if already filled in.
      return;
    }
    if (Record.PointerValueID == StartingValueIDs[0]) {
      Filled1 = true;
      RecordID1 = getCurrentRecordID();
      Address1 = Record.PointeeAddress;
    }
    if (Record.PointerValueID == StartingValueIDs[1]) {
      Filled2 = true;
      RecordID2 = getCurrentRecordID();
      Address2 = Record.PointeeAddress;
    }
    if (Filled1 && Filled2 && Address1 == Address2) {
      StartingRecordIDs.push_back(RecordID1);
      StartingRecordIDs.push_back(RecordID2);
      assert(StartingRecordIDs.size() == 2);
    }
  }

  void initialize() {
    Filled1 = false;
    Filled2 = false;
  }

  bool finalize() {
    return StartingRecordIDs.size() == 2;
  }

 private:
  bool Filled1, Filled2;
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
  string LogFileName = "";
  if (StartingRecordIDs.empty()) {
    // The user specifies staring-value instead of starting-record. Need look
    // for starting-record in the trace.
    errs() << "Finding records of the two input values...\n";
    RecordFinder RF;
    RF.processLog();
    CurrentRecordID = RF.getCurrentRecordID();
    LogFileName = RF.getCurrentFileName();
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
  if (LogFileName != "")
    processLog(LogFileName, true);
  else
    processLog(true);

  if (Trace[0].Active || Trace[1].Active)
    errs() << "Fail to merge!\n";

  if (SliceForReduction) {
    print(errs(), &M);
    if (Merged) {
      // add metadata for values in slice
      for (unsigned PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
        for (unsigned i = 0; i < Trace[PointerLabel].Slice.size(); ++i) {
          Value *V = Trace[PointerLabel].Slice[i].second;
          addMetaData(V, "slice", &M);
          if (i == 0)
            addMetaData(V, "alias", &M);
        }
      }

      // add metadata for related basic blocks
      for (DenseSet<Function *>::iterator I = RelatedFunctions.begin();
           I != RelatedFunctions.end(); ++I) {
        addMetaData(&((*I)->getEntryBlock()), "related", &M);
      }

      // add metadata for executed basic blocks
      for (DenseSet<BasicBlock *>::iterator I = ExecutedBasicBlocks.begin();
           I != ExecutedBasicBlocks.end(); ++I) {
        addMetaData(*I, "executed", &M);
      }
    }
  }

  return true;
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
  O << "RecID\tPtr\tValueID\tGV/Inst/Arg\n";
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

  Function *ContainingFunction = NULL;
  if (Argument *A = dyn_cast<Argument>(V))
    ContainingFunction = A->getParent();
  else if (Instruction *I = dyn_cast<Instruction>(V))
    ContainingFunction = I->getParent()->getParent();

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    if (Trace[PointerLabel].StartingRecordID == CurrentRecordID) {
      // first value found
      Trace[PointerLabel].StartingFunction = ContainingFunction;
      Trace[PointerLabel].Active = true;
      Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                    CurrentRecord.V));
      Trace[PointerLabel].PreviousRecord = CurrentRecord;
      CurrentFunction = ContainingFunction;
      NumContainingSlices++;
    } else if (Trace[PointerLabel].Active) {
      pair<bool, bool> Result = dependsOn(CurrentRecord,
                                          Trace[PointerLabel].PreviousRecord);
      Trace[PointerLabel].Active = Result.second;
      if (Result.first) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.V));
        Trace[PointerLabel].PreviousRecord = CurrentRecord;
        CurrentFunction = ContainingFunction;
        NumContainingSlices++;
      }
    }
  }
  // If two sliced traces meet, we stop tracking
  if (NumContainingSlices == 2) {
    Merged = true;
    if (!SliceForReduction) {
      Trace[0].Active = false;
      Trace[1].Active = false;
    }
  }
}

void TraceSlicer::processEnter(const EnterRecord &Record) {
  CurrentRecordID--;
  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
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
        CurrentFunction = I->getParent()->getParent();
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

  // get all related function calls for reduction
  if (SliceForReduction && PushCallInst) {
    for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
      if (Trace[PointerLabel].Active) {
        Trace[PointerLabel].Slice.push_back(make_pair(CurrentRecordID,
                                                      CurrentRecord.V));
      }
    }
    PushCallInst = false;
    CurrentFunction = I->getParent()->getParent();
  }

  for (int PointerLabel = 0; PointerLabel < 2; ++PointerLabel) {
    // Starting record must be a TopLevel record
    assert(Trace[PointerLabel].StartingRecordID != CurrentRecordID);
    if (Trace[PointerLabel].Active) {
      pair<bool, bool> Result = dependsOn(CurrentRecord,
                                          Trace[PointerLabel].PreviousRecord);
      Trace[PointerLabel].Active = Result.second;
      if (Result.first) {
        if (!SliceForReduction)
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
        if (!SliceForReduction && I->getParent()->getParent() ==
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
  if (SliceForReduction) {
    // record executed basic blocks
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    Value *V = IDA.getValue(Record.ValueID);
    BasicBlock *BB = cast<BasicBlock>(V);
    ExecutedBasicBlocks.insert(BB);

    // record related functions
    if (Trace[0].Active || Trace[1].Active) {
      Function *F = BB->getParent();
      if (&(F->getEntryBlock()) == BB) {
        if (F == CurrentFunction) {
          PushCallInst = true;
          RelatedFunctions.insert(F);
        }
      }
    }
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
    } else if (CS1) {
      // R2 is an external function call
      assert(R1.V == R2.V);
      return make_pair(false, false);
    } else if (dyn_cast<ReturnInst>(R1.V)) {
      return make_pair(true, true);
    } else {
      assert(false);
    }
  } else if (Argument *A = dyn_cast<Argument>(R2.V)) {
    if (CS1) {
      Function *CalledFunction = CS1.getCalledFunction();
      if (CalledFunction && CalledFunction->isDeclaration()) {
        // containing function is called by external function
        return make_pair(false, false);
      }
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

void TraceSlicer::addMetaData(Value *V, string Kind, Module *M) {
  Function *DeclareFn = Intrinsic::getDeclaration(M, Intrinsic::dbg_declare);
  Instruction *Inst = dyn_cast<Instruction>(V);
  if (!Inst) {
    vector<Value *> Args;
    Instruction *InsertBefore;
    if (BasicBlock *BB = dyn_cast<BasicBlock>(V)) {
      // add metadata for basic block
      Args.push_back(MDNode::get(M->getContext(), NULL));
      InsertBefore = BB->getFirstInsertionPt();
    } else {
      Function *F;
      if (Argument *A = dyn_cast<Argument>(V)) {
        // add metadata for argument
        F = A->getParent();
      } else {
        // add metadata for global variable
        F = M->getFunction("main");
        assert(F);
      }
      Args.push_back(MDNode::get(V->getContext(), V));
      InsertBefore = F->getEntryBlock().getFirstInsertionPt();
    }
    Args.push_back(MDNode::get(M->getContext(), NULL));
    Inst = CallInst::Create(DeclareFn, Args, "", InsertBefore);
  }
  Inst->setMetadata(Kind, MDNode::get(M->getContext(), NULL));
}
