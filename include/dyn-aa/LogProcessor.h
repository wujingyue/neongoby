// Author: Jingyue

#ifndef __DYN_AA_LOG_PROCESSOR_H
#define __DYN_AA_LOG_PROCESSOR_H

#include "dyn-aa/LogRecord.h"

namespace dyn_aa {
struct LogProcessor {
  void processLog(bool Reversed = false);
  virtual void processAddrTakenDecl(const AddrTakenDeclLogRecord &) = 0;
  virtual void processTopLevelPointTo(const TopLevelPointToLogRecord &) = 0;
  virtual void processAddrTakenPointTo(const AddrTakenPointToLogRecord &) = 0;

private:
  static bool ReadData(void *P, int Length, bool Reversed, FILE *LogFile);
  static off_t GetFileSize(FILE *LogFile);
  static unsigned EstimateNumRecords(FILE *LogFile);
};
}

#endif
