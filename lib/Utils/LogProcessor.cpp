// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <string>
#include <cstdio>

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/Utils.h"
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
  errs() << "[LogProcessor] Counting the number of records...\n";
  LogRecordType RecordType;
  while (fread(&RecordType, sizeof(LogRecordType), 1, LogFile) == 1) {
    ++NumRecords;
    switch (RecordType) {
      case AddrTakenDecl:
        {
          ++NumAddrTakenDecls;
          int R = fseek(LogFile, sizeof(AddrTakenDeclLogRecord), SEEK_CUR);
          assert(R == 0);
        }
        break;
      case TopLevelPointTo:
        {
          ++NumTopLevelPointTos;
          int R = fseek(LogFile, sizeof(TopLevelPointToLogRecord), SEEK_CUR);
          assert(R == 0);
        }
        break;
      case AddrTakenPointTo:
        {
          ++NumAddrTakenPointTos;
          int R = fseek(LogFile, sizeof(AddrTakenPointToLogRecord), SEEK_CUR);
          assert(R == 0);
        }
        break;
      default:
        fprintf(stderr, "RecordType = %d\n", RecordType);
        assert(false && "Unknown record type");
    }
  }
  assert(NumRecords > 0);
  errs() << "[LogProcessor] Need process " << NumRecords << " records.\n";

  // Set the file position to the beginning.
  rewind(LogFile);

  // Actually process these log records.
  unsigned NumRecordsProcessed = 0;
  DynAAUtils::PrintProgressBar(NumRecordsProcessed, NumRecords);
  while (fread(&RecordType, sizeof(LogRecordType), 1, LogFile) == 1) {
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          size_t R = fread(&Record, sizeof(AddrTakenDeclLogRecord), 1,
                           LogFile);
          assert(R == 1);
          processAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          size_t R = fread(&Record, sizeof(TopLevelPointToLogRecord), 1,
                           LogFile);
          assert(R == 1);
          processTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
          AddrTakenPointToLogRecord Record;
          size_t R = fread(&Record, sizeof(AddrTakenPointToLogRecord), 1,
                           LogFile);
          assert(R == 1);
          processAddrTakenPointTo(Record);
        }
        break;
      default:
        assert(false);
    }
    ++NumRecordsProcessed;
    DynAAUtils::PrintProgressBar(NumRecordsProcessed, NumRecords);
  }
  errs() << "\n";

  fclose(LogFile);
}
