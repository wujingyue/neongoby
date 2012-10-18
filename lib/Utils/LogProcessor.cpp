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

void LogProcessor::processLog(bool Reversed) {
  assert(LogFileName != "" && "Didn't specify the log file.");
  FILE *LogFile = fopen(LogFileName.c_str(), "rb");
  assert(LogFile && "The log file doesn't exist.");

  // Count the records.
  errs() << "[LogProcessor] Counting the number of records...\n";
  LogRecordType RecordType;
  NumRecords = 0;
  while (fread(&RecordType, sizeof RecordType, 1, LogFile) == 1) {
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
        errs() << "RecordType = " << RecordType << "\n";
        errs() << "Current position: " << ftell(LogFile) << " Records read: " <<
                NumRecords << "\n";
        assert(false && "Unknown record type");
    }
    fread(&RecordType, sizeof RecordType, 1, LogFile);
  }
  assert(NumRecords > 0);
  errs() << "[LogProcessor] Need process " << NumRecords << " records.\n";

  if (!Reversed) {
    // Set the file position to the beginning.
    rewind(LogFile);
  } else {
    // Set the file position to the end.
    fseek(LogFile, 0, SEEK_END);
  }

  // Actually process these log records.
  unsigned NumRecordsProcessed = 0;
  DynAAUtils::PrintProgressBar(NumRecordsProcessed, NumRecords);
  while (readData(&RecordType, sizeof RecordType, Reversed, LogFile)) {
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          bool R = readData(&Record, sizeof Record, Reversed, LogFile);
          assert(R == true);
          processAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          bool R = readData(&Record, sizeof Record, Reversed, LogFile);
          assert(R == true);
          processTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
          AddrTakenPointToLogRecord Record;
          bool R = readData(&Record, sizeof Record, Reversed, LogFile);
          assert(R == true);
          processAddrTakenPointTo(Record);
        }
        break;
      default:
        assert(false);
    }
    readData(&RecordType, sizeof RecordType, Reversed, LogFile);
    ++NumRecordsProcessed;
    DynAAUtils::PrintProgressBar(NumRecordsProcessed, NumRecords);
  }
  errs() << "\n";

  fclose(LogFile);
}

bool LogProcessor::readData(void *P, int Length, bool Reversed, FILE *LogFile) {
  if (Reversed) {
    if (fseek(LogFile, -Length, SEEK_CUR) != 0) {
      return false;
    }
  }
  if (fread(P, Length, 1, LogFile) != 1) {
    return false;
  }
  if (Reversed) {
    fseek(LogFile, -Length, SEEK_CUR);
  }
  return true;
}
