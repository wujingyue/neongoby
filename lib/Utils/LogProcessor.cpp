// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

  // Estimate the number of records.
  unsigned EstimatedNumRecords = EstimateNumRecords(LogFile);

  if (Reversed) {
    // Set the file position to the end.
    fseek(LogFile, 0, SEEK_END);
  }

  // Actually process these log records.
  NumRecords = 0;
  DynAAUtils::PrintProgressBar(NumRecords, EstimatedNumRecords);
  LogRecordType RecordType;
  while (ReadData(&RecordType, sizeof RecordType, Reversed, LogFile)) {
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          bool R = ReadData(&Record, sizeof Record, Reversed, LogFile);
          assert(R == true);
          processAddrTakenDecl(Record);
          ++NumAddrTakenDecls;
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          bool R = ReadData(&Record, sizeof Record, Reversed, LogFile);
          assert(R == true);
          processTopLevelPointTo(Record);
          ++NumTopLevelPointTos;
        }
        break;
      case AddrTakenPointTo:
        {
          AddrTakenPointToLogRecord Record;
          bool R = ReadData(&Record, sizeof Record, Reversed, LogFile);
          assert(R == true);
          processAddrTakenPointTo(Record);
          ++NumAddrTakenPointTos;
        }
        break;
      default:
        assert(false);
    }
    ReadData(&RecordType, sizeof RecordType, Reversed, LogFile);
    ++NumRecords;
    DynAAUtils::PrintProgressBar(NumRecords, EstimatedNumRecords);
  }
  if (NumRecords < EstimatedNumRecords) {
    EstimatedNumRecords = NumRecords;
    DynAAUtils::PrintProgressBar(NumRecords, EstimatedNumRecords);
  }
  errs() << "\n";

  fclose(LogFile);
}

bool LogProcessor::ReadData(void *P, int Length, bool Reversed, FILE *LogFile) {
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

off_t LogProcessor::GetFileSize(FILE *LogFile) {
  int FD = fileno(LogFile);
  assert(FD != -1);
  struct stat StatBuf;
  int R = fstat(FD, &StatBuf);
  assert(R != -1);
  return StatBuf.st_size;
}

unsigned LogProcessor::EstimateNumRecords(FILE *LogFile) {
  size_t AverageItemSize = sizeof(LogRecordType) * 2 +
      (sizeof(AddrTakenPointToLogRecord) +
       sizeof(TopLevelPointToLogRecord) +
       sizeof(AddrTakenDeclLogRecord)) / 3;
  unsigned EstimatedNumRecords = GetFileSize(LogFile) / AverageItemSize;
  if (EstimatedNumRecords < 1)
    EstimatedNumRecords = 1;
  errs() << "[LogProcessor] Estimated number of records = " <<
      EstimatedNumRecords << "\n";
  return EstimatedNumRecords;
}
