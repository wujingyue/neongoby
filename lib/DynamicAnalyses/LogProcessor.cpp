// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <string>
#include <cstdio>

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/LogProcessor.h"

using namespace std;
using namespace llvm;
using namespace dyn_aa;

static cl::opt<string> LogFileName(
    "log-file",
    cl::desc("Point-to log file generated "
             "by running the instrumented program"));

STATISTIC(NumAddrTakenDecls, "Number of addr-taken declaration records");
STATISTIC(NumAddrTakenPointTos, "Number of addr-taken point-to records");
STATISTIC(NumTopLevelPointTos, "Number of top-level point-tos records");
STATISTIC(NumRecords, "Number of all records");

void LogProcessor::processLog() {
  assert(LogFileName != "" && "Didn't specify the log file.");
  FILE *LogFile = fopen(LogFileName.c_str(), "rb");
  assert(LogFile && "The log file doesn't exist.");

  LogRecordType RecordType;
  while (fread(&RecordType, sizeof RecordType, 1, LogFile) == 1) {
    if (NumRecords % 1000000 == 0)
      errs() << "Processed " << NumRecords << " records\n";
    ++NumRecords;
    switch (RecordType) {
      case AddrTakenDecl:
        {
          ++NumAddrTakenDecls;
          AddrTakenDeclLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, LogFile) == 1);
          processAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          ++NumTopLevelPointTos;
          TopLevelPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, LogFile) == 1);
          processTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
          ++NumAddrTakenPointTos;
          AddrTakenPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, LogFile) == 1);
          // Do nothing on AddrTakenPointTo.
        }
        break;
      default:
        fprintf(stderr, "RecordType = %d\n", RecordType);
        assert(false && "Unknown record type");
    }
  }

  fclose(LogFile);

  errs() << "Processed " << NumRecords << " records\n";
}
