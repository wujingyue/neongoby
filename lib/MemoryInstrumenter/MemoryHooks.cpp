// Author: Jingyue

#include <pthread.h>

#include <cstdio>
#include <vector>
using namespace std;

#include <boost/unordered_map.hpp>
using namespace boost;

struct AllocInfo {
  AllocInfo(): Version(0), AllocatedBy(-1) {}
  AllocInfo(unsigned Ver, unsigned ValueID):
      Version(Ver), AllocatedBy(ValueID) {}

  unsigned Version;
  unsigned AllocatedBy; // The ID of the value that allocates this byte. 
};

struct Environment {
  Environment() {
    pthread_mutex_init(&Lock, NULL);
    FILE *LogFile = fopen("/tmp/pts", "w");
    fclose(LogFile);
  }
  pthread_mutex_t Lock;
  unordered_map<void *, AllocInfo> AllocTable;
};

Environment *Global;

extern "C" void InitMemHooks() {
  Global = new Environment();
}

extern "C" void HookMemAlloc(unsigned ValueID, void *Start,
                             unsigned long Bound) {
  pthread_mutex_lock(&Global->Lock);
  fprintf(stderr, "%u: HookMemAlloc(%p, %lu)\n", ValueID, Start, Bound);
  for (unsigned long i = 0; i < Bound; ++i) {
    AllocInfo &AI = Global->AllocTable[(void *)((unsigned long)Start + i)];
    ++AI.Version;
    AI.AllocatedBy = ValueID;
  }
  pthread_mutex_unlock(&Global->Lock);
}

extern "C" void HookMemAccess(void *Value, void *Pointer) {
  pthread_mutex_lock(&Global->Lock);
  assert(Global->AllocTable.count(Pointer));
  if (Global->AllocTable.count(Value)) {
    AllocInfo &PointerAllocInfo = Global->AllocTable.at(Pointer);
    AllocInfo &ValueAllocInfo = Global->AllocTable.at(Value);
    FILE *LogFile = fopen("/tmp/pts", "a");
    fprintf(LogFile, "%u: (%p, %u) => %u: (%p, %u)\n",
            PointerAllocInfo.AllocatedBy, Pointer, PointerAllocInfo.Version,
            ValueAllocInfo.AllocatedBy, Value, ValueAllocInfo.Version);
    fclose(LogFile);
  }
  pthread_mutex_unlock(&Global->Lock);
}

// TODO: Unused for now. 
extern "C" void FinalizeMemHooks() {
  delete Global;
}
