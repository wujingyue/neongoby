// Author: Jingyue

#include <pthread.h>

#include <cstdio>
#include <cstring>
#include <vector>
using namespace std;

#include <boost/unordered_map.hpp>
using namespace boost;

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
    FILE *LogFile = fopen("/tmp/pts", "w");
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

extern "C" void HookMemAlloc(unsigned ValueID, void *Start,
                             unsigned long Bound) {
  pthread_mutex_lock(&Global->Lock);
  // fprintf(stderr, "%u: HookMemAlloc(%p, %lu)\n", ValueID, Start, Bound);
  for (unsigned long i = 0; i < Bound; ++i) {
    unsigned long Addr = (unsigned long)Start + i;
    AddrTakenInfo &AI = Global->AddrTakenInfoTable[(void *)Addr];
    ++AI.Version;
    AI.AllocatedBy = ValueID;
    FILE *LogFile = fopen("/tmp/pts", "a");
    fprintf(LogFile, "%p, %u, %u\n", (void *)Addr, AI.Version, ValueID);
    fclose(LogFile);
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
    FILE *LogFile = fopen("/tmp/pts", "a");
    fprintf(LogFile, "%u, %u => %p, %u, %u\n",
            PointerInfo.ValueID, PointerInfo.Version,
            Value, ValueInfo.Version, ValueInfo.AllocatedBy);
    fclose(LogFile);
  }
  pthread_mutex_unlock(&Global->Lock);
}

extern "C" void HookAddrTaken(void *Value, void *Pointer) {
  pthread_mutex_lock(&Global->Lock);
  assert(Global->AddrTakenInfoTable.count(Pointer));
  if (Global->AddrTakenInfoTable.count(Value)) {
    AddrTakenInfo &PointerInfo = Global->AddrTakenInfoTable.at(Pointer);
    AddrTakenInfo &ValueInfo = Global->AddrTakenInfoTable.at(Value);
    FILE *LogFile = fopen("/tmp/pts", "a");
    fprintf(LogFile, "%p, %u, %u => %p, %u, %u\n",
            Pointer, PointerInfo.Version, PointerInfo.AllocatedBy,
            Value, ValueInfo.Version, ValueInfo.AllocatedBy);
    fclose(LogFile);
  }
  pthread_mutex_unlock(&Global->Lock);
}

// TODO: Unused for now. 
extern "C" void FinalizeMemHooks() {
  delete Global;
}
