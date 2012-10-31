// Author: Jingyue

#include "dyn-aa/LogCounter.h"

using namespace dyn_aa;

void LogCounter::processAddrTakenDecl(const AddrTakenDeclLogRecord &) {
  ++NumLogRecords;
}

void LogCounter::processTopLevelPointTo(const TopLevelPointToLogRecord &) {
  ++NumLogRecords;
}

void LogCounter::processAddrTakenPointTo(const AddrTakenPointToLogRecord &) {
  ++NumLogRecords;
}

void LogCounter::processCallInstruction(const CallInstructionLogRecord &) {
  ++NumLogRecords;
}

void LogCounter::processReturnInstruction(const ReturnInstructionLogRecord &) {
  ++NumLogRecords;
}
