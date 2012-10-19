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

  if (Reversed) {
    // Set the file position to the end.
    fseek(LogFile, 0, SEEK_END);
  }

  uint64_t FileSize = GetFileSize(LogFile);
  uint64_t NumBytesRead = 0;
  NumRecords = 0;
  DynAAUtils::PrintProgressBar(0, NumBytesRead, FileSize);
  LogRecordType RecordType;
  while (ReadData(&RecordType, sizeof RecordType, Reversed, LogFile)) {
    uint64_t OldNumBytesRead = NumBytesRead;
    ++NumRecords;
    NumBytesRead += sizeof RecordType;
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          bool R = ReadData(&Record, sizeof Record, Reversed, LogFile);
          assert(R);
          processAddrTakenDecl(Record);
          ++NumAddrTakenDecls;
          NumBytesRead += sizeof Record;
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          bool R = ReadData(&Record, sizeof Record, Reversed, LogFile);
          assert(R);
          processTopLevelPointTo(Record);
          ++NumTopLevelPointTos;
          NumBytesRead += sizeof Record;
        }
        break;
      case AddrTakenPointTo:
        {
          AddrTakenPointToLogRecord Record;
          bool R = ReadData(&Record, sizeof Record, Reversed, LogFile);
          assert(R);
          processAddrTakenPointTo(Record);
          ++NumAddrTakenPointTos;
          NumBytesRead += sizeof Record;
        }
        break;
      default:
        assert(false);
    }
    ReadData(&RecordType, sizeof RecordType, Reversed, LogFile);
    NumBytesRead += sizeof RecordType;
    DynAAUtils::PrintProgressBar(OldNumBytesRead, NumBytesRead, FileSize);
  }
  errs() << "\n";

  assert(NumBytesRead == FileSize && "The file is not completely read.");

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
