#ifndef __DYN_AA_LOG_DUMPER_H
#define __DYN_AA_LOG_DUMPER_H

#include "dyn-aa/LogProcessor.h"

namespace neongoby {
struct LogDumper: public LogProcessor {
  virtual void beforeRecord(const LogRecord &);
  virtual void processMemAlloc(const MemAllocRecord &);
  virtual void processTopLevel(const TopLevelRecord &);
  virtual void processEnter(const EnterRecord &);
  virtual void processStore(const StoreRecord &);
  virtual void processCall(const CallRecord &);
  virtual void processReturn(const ReturnRecord &);
  virtual void processBasicBlock(const BasicBlockRecord &);
};
}

#endif
