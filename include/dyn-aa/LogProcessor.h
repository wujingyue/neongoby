// Author: Jingyue

#ifndef __DYN_AA_LOG_PROCESSOR_H
#define __DYN_AA_LOG_PROCESSOR_H

#include "dyn-aa/LogRecord.h"

namespace dyn_aa {
struct LogProcessor {
  void processLog();
  virtual void processAddrTakenDecl(const AddrTakenDeclLogRecord &) = 0;
  virtual void processTopLevelPointTo(const TopLevelPointToLogRecord &) = 0;
  virtual void processAddrTakenPointTo(const AddrTakenPointToLogRecord &) = 0;

 private:
  void printProgressBar(unsigned Processed, unsigned Total);
};
}

#endif
