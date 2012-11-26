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
  virtual void processMemAlloc(const MemAllocRecord &);
  virtual void processTopLevel(const TopLevelRecord &);
  virtual void processStore(const StoreRecord &);
  virtual void processCall(const CallRecord &);
  virtual void processReturn(const ReturnRecord &);
};
}

static DenseSet<unsigned> TouchedPointers;

void LogDumper::processMemAlloc(const MemAllocRecord &Record) {
  printf("%u: %p, %lu\n", Record.AllocatedBy, Record.Address, Record.Bound);
}

void LogDumper::processTopLevel(const TopLevelRecord &Record) {
  printf("%u => %p", Record.PointerValueID, Record.PointeeAddress);
  if (Record.LoadedFrom) {
    printf(", from %p", Record.LoadedFrom);
  }
  printf("\n");
  TouchedPointers.insert(Record.PointerValueID);
}

void LogDumper::processStore(const StoreRecord &Record) {
  printf("%u: %p => %p\n",
         Record.InstructionID, Record.PointerAddress, Record.PointeeAddress);
}

void LogDumper::processCall(const CallRecord &Record) {
  printf("%u: call\n", Record.InstructionID);
}

void LogDumper::processReturn(const ReturnRecord &Record) {
  printf("%u: return\n", Record.InstructionID);
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Dumps point-to logs");
  LogDumper LD;
  LD.processLog();
  errs() << "# of touched pointers = " << TouchedPointers.size() << "\n";
  return 0;
}
