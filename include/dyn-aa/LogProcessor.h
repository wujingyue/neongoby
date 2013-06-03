#ifndef __DYN_AA_LOG_PROCESSOR_H
#define __DYN_AA_LOG_PROCESSOR_H

#include <pthread.h>

#include <cstdio>

#include "dyn-aa/LogRecord.h"

namespace dyn_aa {
struct LogProcessor {
  LogProcessor(): CurrentRecordID(0) {}
  void processLog(bool Reversed = false);
  bool processLog(const std::string &LogFileName, bool Reversed = false);
  unsigned getCurrentRecordID() const { return CurrentRecordID; }
  std::string getCurrentFileName() const {return CurrentFileName; }

  // initialize is called before processing each log file, and finalize is
  // called after processing each log file.
  virtual void initialize() {}
  virtual bool finalize() { return false; }
  // beforeRecord is called before processing each log record, and afterRecord
  // is called after processing each log record. Therefore, a typical callback
  // flow is:
  //
  // initialize
  // beforeRecord
  // processXXX
  // afterRecord
  // ...
  // beforeRecord
  // processXXX
  // afterRecord
  // finailize
  // initialize
  // ...
  virtual void beforeRecord(const LogRecord &) {}
  virtual void afterRecord(const LogRecord &) {}
  // callback function on each record type
  virtual void processMemAlloc(const MemAllocRecord &) {}
  virtual void processTopLevel(const TopLevelRecord &) {}
  virtual void processEnter(const EnterRecord &) {}
  virtual void processStore(const StoreRecord &) {}
  virtual void processCall(const CallRecord &) {}
  virtual void processReturn(const ReturnRecord &) {}
  virtual void processBasicBlock(const BasicBlockRecord &) {}

 private:
  static bool ReadData(void *P, int Length, bool Reversed, FILE *LogFile);
  static off_t GetFileSize(FILE *LogFile);

  unsigned CurrentRecordID;
  std::string CurrentFileName;
};
}

#endif
