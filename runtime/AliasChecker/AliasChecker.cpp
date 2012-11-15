// vim: sw=2

// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <cstdio>
#include <cstdlib>
#include <queue>
#include <pthread.h>
using namespace std;

class SpinLock {
private:
  pthread_spinlock_t InternalSpinLock;

public:
  SpinLock() {
    pthread_spin_init(&InternalSpinLock, 0);
  }

  void Lock() {
    pthread_spin_lock(&InternalSpinLock);
  }

  void Unlock() {
    pthread_spin_unlock(&InternalSpinLock);
  }

  ~SpinLock() {
    pthread_spin_destroy(&InternalSpinLock);
  }
};

static queue<void*> FreeQueue, DeleteQueue, DeleteArrayQueue;
static unsigned FreeTriggerLimit = 20000;
static unsigned FreeCount = 10000;
static SpinLock FreeLock, ReportLock;

typedef void (*FreeFuncType)(void *Arg);
extern "C" void _ZdlPv(void *MemBlock);
extern "C" void _ZdaPv(void *Array);

extern "C" void ReportMissingAlias(unsigned VIDOfP, unsigned VIDOfQ, void *V) {
  ReportLock.Lock();
  fprintf(stderr, "Missing alias:\n[%u]\n[%u]\n", VIDOfP, VIDOfQ);
  ReportLock.Unlock();
}

extern "C" void SilenceMissingAlias(unsigned VIDOfP, unsigned VIDOfQ, void *V) {
}

extern "C" void AbortIfMissed(void *P, unsigned VIDOfP,
                              void *Q, unsigned VIDOfQ) {
  if (P == Q && P) {
    ReportMissingAlias(VIDOfP, VIDOfQ, P);
    abort();
  }
}

extern "C" void ReportIfMissed(void *P, unsigned VIDOfP,
                               void *Q, unsigned VIDOfQ) {
  if (P == Q && P) {
    ReportMissingAlias(VIDOfP, VIDOfQ, P);
  }
}

extern "C" void SilenceIfMissed(void *P, unsigned VIDOfP,
                                void *Q, unsigned VIDOfQ) {
  if (P == Q && P) {
    SilenceMissingAlias(VIDOfP, VIDOfQ, P);
  }
}

static void RecordItemFreed(void *Item, queue<void*> &Queue, FreeFuncType Free) {
#ifdef DEBUG_FREE_QUEUE
  fprintf(stderr, "free: %p\n", Item);
#endif
  FreeLock.Lock();
  Queue.push(Item);
  if (Queue.size() > FreeTriggerLimit) {
#ifdef DEBUG_FREE_QUEUE
    fprintf(stderr, "limit reached. freeing items\n");
#endif
    for (unsigned i=0; i<FreeCount; i++) {
#ifdef DEBUG_FREE_QUEUE
      fprintf(stderr, "real free: %p\n", Queue.front());
#endif
      Free(Queue.front());
      Queue.pop();
    }
  }
  FreeLock.Unlock();
}

extern "C" void dynaa_free(void *MemBlock) {
  RecordItemFreed(MemBlock, FreeQueue, free);
}

extern "C" void dynaa_delete(void *MemBlock) {
  RecordItemFreed(MemBlock, DeleteQueue, _ZdlPv);
}

extern "C" void dynaa_delete_array(void *Array) {
  RecordItemFreed(Array, DeleteArrayQueue, _ZdaPv);
}
