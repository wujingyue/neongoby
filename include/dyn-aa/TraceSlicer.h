// Author: Junyang

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
struct LogRecordInfo{
  LogRecordInfo(): ArgNo(-1) {}
  unsigned ValueID;
  // for CallSite and Argument
  int ArgNo;
  // for LoadInst and StoreInst
  void *PointerAddress;
  // for PHI and SelectInst
  void *PointeeAddress;
};

struct PointerTrace{
  PointerTrace(): Active(false) {}

  unsigned StartingRecordID;
  Function *StartingFunction;
  bool Active;
  LogRecordInfo PreviousRecord;
  vector<pair<unsigned, unsigned> > Slice;
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

  pair<bool, bool> dependsOn(LogRecordInfo &PreviousRecord,
                             LogRecordInfo &CurrentRecord);
  Value *getLatestCommonAncestor();

 private:
  void printTrace(raw_ostream &O,
                  pair<unsigned, unsigned> TraceRecord,
                  int PointerLabel) const;

  PointerTrace Trace[2];
  unsigned CurrentRecordID;
};
}

#endif
