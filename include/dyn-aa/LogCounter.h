#ifndef __DYN_AA_LOG_COUNTER_H
#define __DYN_AA_LOG_COUNTER_H

#include "dyn-aa/LogProcessor.h"

namespace neongoby {
struct LogCounter: public LogProcessor {
  unsigned getNumLogRecords() const { return getCurrentRecordID(); }
};
}

#endif
