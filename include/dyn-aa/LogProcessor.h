#ifndef __DYN_AA_LOG_PROCESSOR_H
#define __DYN_AA_LOG_PROCESSOR_H

#include <cstdio>

#include "dyn-aa/LogRecord.h"

namespace dyn_aa {
struct LogProcessor {
  LogProcessor(): CurrentRecordID(0) {}

  void processLog(bool Reversed = false);
  unsigned getCurrentRecordID() const { return CurrentRecordID; }

  // TODO: We should have a common ancestor of all these records, and provide
  // a common processRecord interface.
  virtual void processMemAlloc(const MemAllocRecord &);
  virtual void processTopLevel(const TopLevelRecord &);
  virtual void processStore(const StoreRecord &);
  virtual void processCall(const CallRecord &);
  virtual void processReturn(const ReturnRecord &);

private:
  static bool ReadData(void *P, int Length, bool Reversed, FILE *LogFile);
  static off_t GetFileSize(FILE *LogFile);

  unsigned CurrentRecordID;
};
}

#endif
