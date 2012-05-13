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

  // Count the records.
  LogRecordType RecordType;
  while (fread(&RecordType, sizeof RecordType, 1, LogFile) == 1) {
    ++NumRecords;
    switch (RecordType) {
      case AddrTakenDecl:
        ++NumAddrTakenDecls;
        assert(fseek(LogFile, sizeof(AddrTakenDeclLogRecord), SEEK_CUR) == 0);
        break;
      case TopLevelPointTo:
        ++NumTopLevelPointTos;
        assert(fseek(LogFile, sizeof(TopLevelPointToLogRecord), SEEK_CUR) == 0);
        break;
      case AddrTakenPointTo:
        ++NumAddrTakenPointTos;
        assert(fseek(LogFile, sizeof(AddrTakenPointToLogRecord),
                     SEEK_CUR) == 0);
        break;
      default:
        fprintf(stderr, "RecordType = %d\n", RecordType);
        assert(false && "Unknown record type");
    }
  }
  assert(NumRecords > 0);
  errs() << "Need process " << NumRecords << " records.\n";

  // Set the file position to the beginning.
  rewind(LogFile);

  // Actually process these log records.
  unsigned NumRecordsProcessed = 0;
  printProgressBar(NumRecordsProcessed, NumRecords);
  while (fread(&RecordType, sizeof RecordType, 1, LogFile) == 1) {
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, LogFile) == 1);
          processAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, LogFile) == 1);
          processTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
          AddrTakenPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, LogFile) == 1);
          processAddrTakenPointTo(Record);
        }
        break;
      default:
        assert(false);
    }
    ++NumRecordsProcessed;
    printProgressBar(NumRecordsProcessed, NumRecords);
  }
  errs() << "\n";

  fclose(LogFile);
}

void LogProcessor::printProgressBar(unsigned Processed, unsigned Total) {
  assert(Processed >= 0 && Processed <= Total);
  assert(Total > 0);

  errs().changeColor(raw_ostream::BLUE);
  if (Processed == 0) {
    errs() << " [0%]";
  } else {
    unsigned CurrentPercentage = Processed * 10 / Total;
    unsigned OldPercentage = (Processed - 1) * 10 / Total;
    if (CurrentPercentage != OldPercentage)
      errs() << " [" << CurrentPercentage * 10 << "%]";
  }
  errs().resetColor();
}
