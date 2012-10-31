// Author: Jingyue

#ifndef __DYN_AA_LOG_COUNTER_H
#define __DYN_AA_LOG_COUNTER_H

#include "dyn-aa/LogProcessor.h"

namespace dyn_aa {
struct LogCounter: public LogProcessor {
  LogCounter(): NumLogRecords(0) {}
  virtual void processAddrTakenDecl(const AddrTakenDeclLogRecord &);
  virtual void processTopLevelPointTo(const TopLevelPointToLogRecord &);
  virtual void processAddrTakenPointTo(const AddrTakenPointToLogRecord &);
  virtual void processCallInstruction(const CallInstructionLogRecord &);
  virtual void processReturnInstruction(const ReturnInstructionLogRecord &);
  unsigned getNumLogRecords() const { return NumLogRecords; }

 private:
  unsigned NumLogRecords;
};
}

#endif
