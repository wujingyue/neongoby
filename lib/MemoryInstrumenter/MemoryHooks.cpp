// Author: Jingyue

// Three types of messages: 
// 1) Declare an addr-taken variable: addr, ver, allocator
// 2) Top-level point to addr-taken: vid, ver => addr, ver, allocator
// 3) Addr-taken point to addr-taken: addr, ver, allocator => addr, ver,
//    allocator
// 
// The third type of messages is not necessary for constructing a traditional
// point-to graph, because users query with top-level variables only. However,
// we put it there because we want to observe the shape. 

#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <vector>
#include <cassert>
#include <unordered_map>
using namespace std;

#include "dyn-aa/LogRecord.h"
using namespace dyn_aa;

struct AddrTakenInfo {
  AddrTakenInfo(): Version(0), AllocatedBy(-1) {}

  unsigned Version;
  unsigned AllocatedBy; // The ID of the value that allocates this byte. 
};

struct TopLevelInfo {
  TopLevelInfo(): Version(0), ValueID(-1) {}

  unsigned Version;
  unsigned ValueID;
};

struct Environment {
  Environment() {
    pthread_mutex_init(&Lock, NULL);
    FILE *LogFile = fopen("/tmp/pts", "wb");
    fclose(LogFile);
  }
  pthread_mutex_t Lock;
  unordered_map<void *, AddrTakenInfo> AddrTakenInfoTable;
  unordered_map<unsigned, TopLevelInfo> TopLevelInfoTable;
};

Environment *Global;

extern "C" void InitMemHooks() {
  Global = new Environment();
}

// Must be called with Global->Lock held. 
template<class T>
void PrintLogRecord(LogRecordType RecordType, const T &Record) {
  FILE *LogFile = fopen("/tmp/pts", "ab");
  fwrite(&RecordType, sizeof RecordType, 1, LogFile);
  fwrite(&Record, sizeof Record, 1, LogFile);
  fclose(LogFile);
}

extern "C" void HookMemAlloc(unsigned ValueID, void *Start,
                             unsigned long Bound) {
  pthread_mutex_lock(&Global->Lock);
  // fprintf(stderr, "%u: HookMemAlloc(%p, %lu)\n", ValueID, Start, Bound);
  for (unsigned long i = 0; i < Bound; ++i) {
    unsigned long Addr = (unsigned long)Start + i;
    AddrTakenInfo &AI = Global->AddrTakenInfoTable[(void *)Addr];
    ++AI.Version;
    AI.AllocatedBy = ValueID;
    PrintLogRecord(AddrTakenDecl,
                   AddrTakenDeclLogRecord((void *)Addr,
                                                 AI.Version,
                                                 ValueID));
  }
  pthread_mutex_unlock(&Global->Lock);
}

extern "C" void HookMainArgsAlloc(int Argc, char *Argv[],
                                  unsigned ArgvValueID) {
  HookMemAlloc(ArgvValueID, Argv, Argc * sizeof(char *));
  for (int i = 0; i < Argc; ++i)
    HookMemAlloc(-1, Argv[i], strlen(Argv[i]) + 1); // ends with '\0'
}

extern "C" void HookTopLevel(void *Value, unsigned ValueID) {
  pthread_mutex_lock(&Global->Lock);
  // fprintf(stderr, "HookTopLevel(%p, %u)\n", Value, ValueID);
  if (Global->AddrTakenInfoTable.count(Value)) {
    TopLevelInfo &PointerInfo = Global->TopLevelInfoTable[ValueID];
    ++PointerInfo.Version;
    PointerInfo.ValueID = ValueID;
    AddrTakenInfo &ValueInfo = Global->AddrTakenInfoTable.at(Value);
    PrintLogRecord(TopLevelPointTo,
                   TopLevelPointToLogRecord(ValueID, PointerInfo.Version,
                                            Value, ValueInfo.Version));
  }
  pthread_mutex_unlock(&Global->Lock);
}

extern "C" void HookAddrTaken(void *Value, void *Pointer) {
  pthread_mutex_lock(&Global->Lock);
  assert(Global->AddrTakenInfoTable.count(Pointer));
  if (Global->AddrTakenInfoTable.count(Value)) {
    AddrTakenInfo &PointerInfo = Global->AddrTakenInfoTable.at(Pointer);
    AddrTakenInfo &ValueInfo = Global->AddrTakenInfoTable.at(Value);
    PrintLogRecord(AddrTakenPointTo,
                   AddrTakenPointToLogRecord(Pointer, PointerInfo.Version,
                                             Value, ValueInfo.Version));
  }
  pthread_mutex_unlock(&Global->Lock);
}

// TODO: Unused for now. 
extern "C" void FinalizeMemHooks() {
  delete Global;
}
