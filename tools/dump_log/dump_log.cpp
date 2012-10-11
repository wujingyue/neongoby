// Author: Jingyue

// Print the point-to log in a readable format. 

#include <cstdio>
#include <cassert>

#include "llvm/Support/CommandLine.h"

#include "dyn-aa/LogProcessor.h"

using namespace std;
using namespace llvm;
using namespace dyn_aa;

namespace dyn_aa {
struct LogDumper: public LogProcessor {
  virtual void processAddrTakenDecl(const AddrTakenDeclLogRecord &);
  virtual void processTopLevelPointTo(const TopLevelPointToLogRecord &);
  virtual void processAddrTakenPointTo(const AddrTakenPointToLogRecord &);
};
}

void LogDumper::processAddrTakenDecl(const AddrTakenDeclLogRecord &Record) {
  printf("%u: %p, %lu\n", Record.AllocatedBy, Record.Address, Record.Bound);
}

void LogDumper::processTopLevelPointTo(const TopLevelPointToLogRecord &Record) {
  printf("%u => %p\n", Record.PointerValueID, Record.PointeeAddress);
}

void LogDumper::processAddrTakenPointTo(
    const AddrTakenPointToLogRecord &Record) {
  printf("%p => %p\n", Record.PointerAddress, Record.PointeeAddress);
}

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Dumps point-to logs");
  LogDumper LD;
  LD.processLog();
  return 0;
}
