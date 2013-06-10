// Print the point-to log in a readable format. 

#include <cstdio>
#include <cassert>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/typedefs.h"

#include "dyn-aa/LogDumper.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

void LogDumper::beforeRecord(const LogRecord &Record) {
  switch (Record.RecordType) {
    case LogRecord::MemAlloc  : printf("[   alloc] "); break;
    case LogRecord::TopLevel  : printf("[ pointer] "); break;
    case LogRecord::Enter     : printf("[   enter] "); break;
    case LogRecord::Store     : printf("[   store] "); break;
    case LogRecord::Call      : printf("[    call] "); break;
    case LogRecord::Return    : printf("[  return] "); break;
    case LogRecord::BasicBlock: printf("[      bb] "); break;
  }
}

void LogDumper::processMemAlloc(const MemAllocRecord &Record) {
  printf("%u: %p, %lu\n", Record.AllocatedBy, Record.Address, Record.Bound);
}

void LogDumper::processTopLevel(const TopLevelRecord &Record) {
  printf("%u => %p", Record.PointerValueID, Record.PointeeAddress);
  if (Record.LoadedFrom) {
    printf(", from %p", Record.LoadedFrom);
  }
  printf("\n");
}

void LogDumper::processEnter(const EnterRecord &Record) {
  printf("%u\n", Record.FunctionID);
}

void LogDumper::processStore(const StoreRecord &Record) {
  printf("%u: %p => %p\n",
         Record.InstructionID,
         Record.PointerAddress,
         Record.PointeeAddress);
}

void LogDumper::processCall(const CallRecord &Record) {
  printf("%u\n", Record.InstructionID);
}

void LogDumper::processReturn(const ReturnRecord &Record) {
  printf("%u\n", Record.InstructionID);
}

void LogDumper::processBasicBlock(const BasicBlockRecord &Record) {
  printf("%u: bb\n", Record.ValueID);
}
