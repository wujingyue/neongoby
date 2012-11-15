#ifndef __DYN_AA_TRACE_SLICER_H
#define __DYN_AA_TRACE_SLICER_H

#include <utility>
#include <vector>

#include "llvm/Pass.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include "rcs/typedefs.h"

#include "dyn-aa/LogRecord.h"
#include "dyn-aa/LogProcessor.h"
#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;

namespace dyn_aa {
struct TraceState{
  TraceState() {
    Action = TopLevelPointTo;
    End = false;
  }

  // Indicates where the trace starts
  unsigned StartingRecordID;
  // The containing function of start pointer for printing ReturnInst
  Function *StartingFunction;

  // Indicates whether go on following the trace
  bool End;

  // Stores the sliced trace, key is RecordID, value is ValueID
  vector<pair<unsigned, unsigned> > Trace;

  // Indicates type of log record to deal with:
  // If Action is AddrTakenPointTo, TraceSlicer will only track Address;
  // If Action is TopLevelPointTo, if previous instruction is Select/PHI,
  // TraceSlicer will track ValueIDCandidates and Address first, and then
  // track ValueID; otherwise, TraceSlicer will only track ValueID;
  // If Action is CallInstruction, ArgNo is used;
  // If Action is ReturnInstruction, StartingFunction is used.
  LogRecordType Action;

  // All possible source pointers of PHI and Select for TopLevelPointTo record.
  // If ValueIDCandidates is empty, TraceSlicer is not dealing with PHI or Select
  DenseSet<unsigned> ValueIDCandidates;

  // Value ID of dest pointer for AddrTakenPointTo record
  unsigned ValueID;

  // Address of pointer for AddrTakenPointTo record
  void *Address;

  // Index of argument for CallInstruction record
  unsigned ArgNo;
};

struct TraceSlicer: public ModulePass, public LogProcessor {
  static char ID;

  TraceSlicer(): ModulePass(ID) {}
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual void print(raw_ostream &O, const Module *M) const;

  // Interfaces of LogProcessor.
  void processAddrTakenDecl(const AddrTakenDeclLogRecord &Record);
  void processTopLevelPointTo(const TopLevelPointToLogRecord &Record);
  void processAddrTakenPointTo(const AddrTakenPointToLogRecord &Record);
  void processCallInstruction(const CallInstructionLogRecord &Record);
  void processReturnInstruction(const ReturnInstructionLogRecord &Record);

  Value *getLatestCommonAncestor();

 private:
  void trackSourcePointer(TraceState &TS,
                         const TopLevelPointToLogRecord &Record);
  void printTrace(raw_ostream &O,
                  pair<unsigned, unsigned> TraceRecord,
                  int PointerLabel) const;
  bool isLive(int PointerLabel);

  TraceState CurrentState[2];
  unsigned CurrentRecordID;
};
}

#endif
