#define DEBUG_TYPE "dyn-aa"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <cstdio>
#include <iostream>

#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/Utils.h"
#include "dyn-aa/LogProcessor.h"

using namespace std;
using namespace llvm;
using namespace neongoby;

static cl::list<string> LogFileNames(
    "log-file",
    cl::desc("Point-to log files generated "
             "by running the instrumented program"));

STATISTIC(NumMemAllocRecords, "Number of memory allocation records");
STATISTIC(NumTopLevelRecords, "Number of top-level records");
STATISTIC(NumEnterRecords, "Number of enter records");
STATISTIC(NumStoreRecords, "Number of store records");
STATISTIC(NumCallRecords, "Number of call records");
STATISTIC(NumReturnRecords, "Number of return records");
STATISTIC(NumBasicBlockRecords, "Number of basic block records");
STATISTIC(NumRecords, "Number of all records");

void LogProcessor::processLog(bool Reversed) {
  assert(LogFileNames.size() && "Didn't specify the log file.");
  for (unsigned i = 0; i < LogFileNames.size(); i++) {
    processLog(LogFileNames[i], Reversed);
  }
}

void LogProcessor::processLog(const std::string &LogFileName, bool Reversed) {
  FILE *LogFile = fopen(LogFileName.c_str(), "rb");
  assert(LogFile && "The log file doesn't exist.");
  errs().changeColor(raw_ostream::BLUE);
  errs() << "Processing log " << LogFileName << " ...\n";
  errs().resetColor();

  if (Reversed) {
    // Set the file position to the end.
    fseek(LogFile, 0, SEEK_END);
  }

  initialize();

  uint64_t FileSize = GetFileSize(LogFile);
  uint64_t NumBytesRead = 0;
  NumRecords = 0;
  CurrentRecordID = 0;
  DynAAUtils::PrintProgressBar(0, NumBytesRead, FileSize);
  LogRecord Record;
  while (ReadData(&Record, sizeof Record, Reversed, LogFile)) {
    uint64_t OldNumBytesRead = NumBytesRead;
    ++NumRecords;
    NumBytesRead += sizeof Record;
    beforeRecord(Record);
    switch (Record.RecordType) {
      case LogRecord::MemAlloc:
        processMemAlloc(Record.MAR);
        ++NumMemAllocRecords;
        break;
      case LogRecord::TopLevel:
        processTopLevel(Record.TLR);
        ++NumTopLevelRecords;
        break;
      case LogRecord::Enter:
        processEnter(Record.ER);
        ++NumEnterRecords;
        break;
      case LogRecord::Store:
        processStore(Record.SR);
        ++NumStoreRecords;
        break;
      case LogRecord::Call:
        processCall(Record.CR);
        ++NumCallRecords;
        break;
      case LogRecord::Return:
        processReturn(Record.RR);
        ++NumReturnRecords;
        break;
      case LogRecord::BasicBlock:
        processBasicBlock(Record.BBR);
        ++NumBasicBlockRecords;
        break;
    }
    afterRecord(Record);
    ++CurrentRecordID;
    DynAAUtils::PrintProgressBar(OldNumBytesRead, NumBytesRead, FileSize);
  }
  errs() << "\n";

  assert(NumBytesRead <= FileSize);
  if (NumBytesRead < FileSize) {
    errs().changeColor(raw_ostream::RED);
    errs() << "The log file is broken, probably because ";
    errs() << "the instrumented program might not exit normally. ";
    errs() << "Try to process as much log as possible.\n";
    errs().resetColor();
  }

  finalize();

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
