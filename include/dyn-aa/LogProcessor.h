#ifndef __DYN_AA_LOG_PROCESSOR_H
#define __DYN_AA_LOG_PROCESSOR_H

#include <pthread.h>

#include <cstdio>

#include "dyn-aa/LogRecord.h"

namespace dyn_aa {
struct LogProcessor {
  LogProcessor(): CurrentRecordID(0) {}

  void processLog(bool Reversed = false);
  unsigned getCurrentRecordID() const { return CurrentRecordID; }
  pthread_t getCurrentThreadID() const { return CurrentThreadID; }

  // By default, these call-back functions do nothing.
  virtual void beforeProcess(const LogRecord &) {}
  virtual void processMemAlloc(const MemAllocRecord &) {}
  virtual void processTopLevel(const TopLevelRecord &) {}
  virtual void processEnter(const EnterRecord &) {}
  virtual void processStore(const StoreRecord &) {}
  virtual void processCall(const CallRecord &) {}
  virtual void processReturn(const ReturnRecord &) {}
  virtual void processBasicBlock(const BasicBlockRecord &) {}
  virtual void afterProcess(const LogRecord &) {}

 private:
  void processLog(const std::string &LogFileName, bool Reversed);
  static bool ReadData(void *P, int Length, bool Reversed, FILE *LogFile);
  static off_t GetFileSize(FILE *LogFile);

  unsigned CurrentRecordID;
  pthread_t CurrentThreadID;
};
}

#endif
