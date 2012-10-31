// Author: Jingyue

// Print the point-to log in a readable format. 

#include <cstdio>
#include <cassert>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/typedefs.h"

#include "dyn-aa/LogProcessor.h"
#include "dyn-aa/LogCounter.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

namespace dyn_aa {
struct LogDumper: public LogProcessor {
  virtual void processAddrTakenDecl(const AddrTakenDeclLogRecord &);
  virtual void processTopLevelPointTo(const TopLevelPointToLogRecord &);
  virtual void processAddrTakenPointTo(const AddrTakenPointToLogRecord &);
  virtual void processCallInstruction(const CallInstructionLogRecord &);
  virtual void processReturnInstruction(const ReturnInstructionLogRecord &);
};
}

static DenseSet<unsigned> TouchedPointers;

void LogDumper::processAddrTakenDecl(const AddrTakenDeclLogRecord &Record) {
  printf("%u: %p, %lu\n", Record.AllocatedBy, Record.Address, Record.Bound);
}

void LogDumper::processTopLevelPointTo(const TopLevelPointToLogRecord &Record) {
  printf("%u => %p", Record.PointerValueID, Record.PointeeAddress);
  if (Record.LoadedFrom) {
    printf(", from %p", Record.LoadedFrom);
  }
  printf("\n");
  TouchedPointers.insert(Record.PointerValueID);
}

void LogDumper::processAddrTakenPointTo(
    const AddrTakenPointToLogRecord &Record) {
  printf("%u: %p => %p\n",
         Record.InstructionID, Record.PointerAddress, Record.PointeeAddress);
}

void LogDumper::processCallInstruction(
    const CallInstructionLogRecord &Record) {
  printf("%u: call\n", Record.InstructionID);
}

void LogDumper::processReturnInstruction(
    const ReturnInstructionLogRecord &Record) {
  printf("%u: return\n", Record.InstructionID);
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Dumps point-to logs");
  LogDumper LD;
  LD.processLog();
  errs() << "# of touched pointers = " << TouchedPointers.size() << "\n";
  return 0;
}
