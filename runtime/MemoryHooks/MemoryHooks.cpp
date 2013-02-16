// vim: sw=2

// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <pthread.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cassert>
#include <signal.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <stack>
#include <sys/file.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "rcs/IDAssigner.h"

#include "dyn-aa/LogRecord.h"

using namespace std;
using namespace rcs;
using namespace dyn_aa;

static __thread FILE *MyLogFile = NULL;
static vector<FILE *> LogFiles;
static pthread_mutex_t Lock = PTHREAD_MUTEX_INITIALIZER;
static __thread int NumActualArgs;
// These two thread-specific flags are used to workaround the issue with signal
// handling.
static __thread bool IsLogging = false;
static __thread bool DisableLogging = false;

// TODO: store logs from each run in a separate directory named by the
// timestamp. e.g., /tmp/pts-20121113-100535.
string GetLogFileName(pid_t ThreadID) {
  const char *LogFileEnv = getenv("LOG_FILE");
  string LogFileName;
  if (!LogFileEnv) {
    LogFileName = "/tmp/pts";
  } else {
    LogFileName = LogFileEnv;
  }
  ostringstream OS;
  OS << LogFileName << "-" << ThreadID;
  return OS.str();
}

string GetLogFileName() {
  pid_t ThreadID = syscall(SYS_gettid);
  return GetLogFileName(ThreadID);
}

// TODO: The Append flag is not necessary. We could just uniformly use "ab".
void OpenLogFile(bool Append) {
  MyLogFile = fopen(GetLogFileName().c_str(), Append ? "ab" : "wb");
  if (!MyLogFile)
    perror("fopen");
  // cerr << "[" << getpid() << "] open " << GetLogFileName() << " as " << MyLogFile << "\n";
  assert(MyLogFile);
  pthread_mutex_lock(&Lock);
  LogFiles.push_back(MyLogFile);
  pthread_mutex_unlock(&Lock);
}

void OpenLogFileIfNecessary() {
  if (!MyLogFile)
    OpenLogFile(false);
}

extern "C" void FinalizeMemHooks() {
  pthread_mutex_lock(&Lock);
  for (size_t i = 0; i < LogFiles.size(); ++i) {
    // cerr << "[" << getpid() << "] close " << LogFiles[i] << "\n";
    assert(LogFiles[i]);
    fclose(LogFiles[i]);
  }
  pthread_mutex_unlock(&Lock);
}

extern "C" void InitMemHooks() {
  atexit(FinalizeMemHooks);
}

void PrintLogRecord(const LogRecord &Record) {
  // FIXME: Signal handler can happen anytime even if during the fwrite, causing
  // the log to be broken. To workaround this issue, PrintLogRecord sets
  // IsLogging at the beginning, and resets it at the end. If PrintLogRecord
  // is entered with IsLogging on, the thread is probably inside a signal
  // handler. In that case, we simply disable future logging. This approach is
  // sure problematic because it misses logs, but it is not a big deal for now,
  // because the signal is usually generated at the end of the execution when
  // the user wants to terminate the server. A better approach would be to
  // figure out statically which functions are signal handlers, and only disable
  // logging inside the life cycles of these signal handlers.
  if (IsLogging)
    DisableLogging = true;
  if (DisableLogging)
    return;

  IsLogging = true;
  OpenLogFileIfNecessary();
  size_t NumBytesWritten = fwrite(&Record, sizeof Record, 1, MyLogFile);
  assert(NumBytesWritten == 1);
  IsLogging = false;
}

extern "C" void HookBeforeFork() {
  // FIXME: MyLogFile can be empty if no log is written before forking.
  // We assume there is only one running thread at the time of forking.
  // Therefore, we don't have to protect LogFiles through the entire forking
  // process.
  for (size_t i = 0; i < LogFiles.size(); ++i) {
    assert(LogFiles[i]);
    fflush(LogFiles[i]);
  }
  assert(find(LogFiles.begin(), LogFiles.end(), MyLogFile) != LogFiles.end());
  flock(fileno(MyLogFile), LOCK_EX);
}

extern "C" void HookAfterFork(int Result) {
  if (Result == 0) {
    // child process: wait for copy to finish, then open the log file
    assert(find(LogFiles.begin(), LogFiles.end(), MyLogFile) != LogFiles.end());
    for (size_t i = 0; i < LogFiles.size(); ++i) {
      assert(LogFiles[i]);
      fclose(LogFiles[i]);
    }

    string ParentLogFileName = GetLogFileName(getppid());
    FILE *ParentLogFile = fopen(ParentLogFileName.c_str(), "rb");
    if (ParentLogFile) {
      // TODO: in what situation would ParentLogFile be NULL? Should we fail
      // somehow in this situation?
      flock(fileno(ParentLogFile), LOCK_EX);
      flock(fileno(ParentLogFile), LOCK_UN);
      fclose(ParentLogFile);
    }

    // The child process inherits LogFiles from the parent process, which are
    // no longer valid. Therefore, we clear them.
    // Grabbing the mutex here isn't necessary, because there should only be one
    // thread running right after the fork.
    LogFiles.clear();
    // Although unlikely, DisableLogging may be set by the parent process. Reset
    // it to false for this child process.
    DisableLogging = false;
    OpenLogFile(true);
    assert(LogFiles.size() == 1);
  } else {
    // parent process: duplicate the log file, then unlock it
    string ParentLogFileName = GetLogFileName();
    string ChildLogFileName = GetLogFileName(Result);
    string CmdLine = "cp " + ParentLogFileName + " " + ChildLogFileName;
    int Ret = system(CmdLine.c_str());
    assert(Ret == 0);
    flock(fileno(MyLogFile), LOCK_UN);
  }
}

extern "C" void HookMemAlloc(unsigned ValueID,
                             void *StartAddr,
                             unsigned long Bound) {
  // Bound is sometimes zero for array allocation.
  if (Bound > 0) {
    // fprintf(stderr, "%u: HookMemAlloc(%p, %lu)\n", ValueID, StartAddr, Bound);
    LogRecord Record;
    Record.RecordType = LogRecord::MemAlloc;
    Record.MAR.Address = StartAddr;
    Record.MAR.Bound = Bound;
    Record.MAR.AllocatedBy = ValueID;
    PrintLogRecord(Record);
  }
}

extern "C" void HookMainArgsAlloc(int Argc, char *Argv[],
                                  unsigned ArgvValueID) {
  HookMemAlloc(ArgvValueID, Argv, Argc * sizeof(char *));
  for (int i = 0; i < Argc; ++i)
    HookMemAlloc(-1, Argv[i], strlen(Argv[i]) + 1); // ends with '\0'
}

extern "C" void HookTopLevel(void *Value, void *Pointer, unsigned ValueID) {
  // fprintf(stderr, "HookTopLevel(%p, %u)\n", Value, ValueID);
  LogRecord Record;
  Record.RecordType = LogRecord::TopLevel;
  Record.TLR.PointerValueID = ValueID;
  Record.TLR.PointeeAddress = Value;
  Record.TLR.LoadedFrom = Pointer;
  PrintLogRecord(Record);
}

extern "C" void HookEnter(unsigned FuncID) {
  LogRecord Record;
  Record.RecordType = LogRecord::Enter;
  Record.ER.FunctionID = FuncID;
  PrintLogRecord(Record);
}

extern "C" void HookStore(void *Value, void *Pointer, unsigned InsID) {
  // fprintf(stderr, "HookStore(%p, %p, %u)\n", Value, Pointer, InsID);
  LogRecord Record;
  Record.RecordType = LogRecord::Store;
  Record.SR.PointerAddress = Pointer;
  Record.SR.PointeeAddress = Value;
  Record.SR.InstructionID = InsID;
  PrintLogRecord(Record);
}

extern "C" void HookCall(unsigned InsID, int NumArgs) {
  // fprintf(stderr, "HookCall(%u, %d)\n", InsID, NumArgs);
  LogRecord Record;
  Record.RecordType = LogRecord::Call;
  Record.CR.InstructionID = InsID;
  PrintLogRecord(Record);
  NumActualArgs = NumArgs;
}

extern "C" void HookReturn(unsigned InsID) {
  // fprintf(stderr, "HookReturn(%u)\n", InsID);
  LogRecord Record;
  Record.RecordType = LogRecord::Return;
  Record.RR.InstructionID = InsID;
  PrintLogRecord(Record);
}

extern "C" void HookVAStart(void *VAList) {
  struct TPVAList {
    int32_t gp_offset;
    int32_t fp_offset;
    int8_t *overflow_arg_area;
    int8_t *reg_save_area;
  } *PVAList;
  PVAList = (TPVAList *)VAList;

  // FIXME: we don't know if this is correct.
  const int NumIntRegs = 6;
  const int NumXMMRegs = 8;
  // Allocating register saved area.
  // FIXME: use a correct ID.
  HookMemAlloc(IDAssigner::InvalidID,
               PVAList->reg_save_area,
               NumIntRegs * 8 + NumXMMRegs * 16);
  if (NumActualArgs > 6) {
    // Allocating overflow area.
    // FIXME: use a correct ID.
    HookMemAlloc(IDAssigner::InvalidID,
                 PVAList->overflow_arg_area,
                 (NumActualArgs - 6) * 8);
  }
}
