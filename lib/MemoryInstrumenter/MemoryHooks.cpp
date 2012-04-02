// Author: Jingyue

#include <pthread.h>

#include <cstdio>
#include <vector>
using namespace std;

#include <boost/unordered_map.hpp>
using namespace boost;

typedef pair<void *, int> MemNode;

struct Environment {
  Environment() {
    pthread_mutex_init(&Lock, NULL);
    FILE *LogFile = fopen("/tmp/pts", "w");
    fclose(LogFile);
  }
  pthread_mutex_t Lock;
  unordered_map<void *, unsigned> MemVersion;
};

Environment *Global;

extern "C" void InitMemHooks() {
  Global = new Environment();
}

extern "C" void HookMemAlloc(void *Start, unsigned long Bound) {
  pthread_mutex_lock(&Global->Lock);
  fprintf(stderr, "HookMemAlloc(%p, %lu)\n", Start, Bound);
  for (unsigned long i = 0; i < Bound; ++i)
    ++Global->MemVersion[(void *)((unsigned long)Start + i)];
  pthread_mutex_unlock(&Global->Lock);
}

extern "C" void HookMemAccess(void *Value, void *Pointer) {
  pthread_mutex_lock(&Global->Lock);
  assert(Global->MemVersion.count(Pointer));
  if (Global->MemVersion.count(Value)) {
    FILE *LogFile = fopen("/tmp/pts", "a");
    fprintf(LogFile, "(%p, %u) => (%p, %u)\n",
            Pointer, Global->MemVersion[Pointer],
            Value, Global->MemVersion[Value]);
    fclose(LogFile);
  }
  pthread_mutex_unlock(&Global->Lock);
}

// TODO: Unused for now. 
extern "C" void FinalizeMemHooks() {
  delete Global;
}
