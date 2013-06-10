// vim: sw=2

// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <pthread.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <sstream>
#include <string>
using namespace std;

struct Environment {
  Environment(): ReportFile(NULL) {
    pthread_spin_init(&FreeLock, 0);
    pthread_mutex_init(&ReportLock, NULL);
    ReportFile = fopen(GetReportFileName().c_str(), "wb");
  }

  ~Environment() {
    pthread_spin_destroy(&FreeLock);
    pthread_mutex_destroy(&ReportLock);
    assert(ReportFile);
    fclose(ReportFile);
  }

  static string GetReportFileName() {
    ostringstream OSS;
    OSS << "/tmp/report-" << getpid();
    return OSS.str();
  }

  static const unsigned QueueSize;
  queue<void *> FreeQueue, DeleteQueue, DeleteArrayQueue;
  pthread_spinlock_t FreeLock;
  pthread_mutex_t ReportLock;
  FILE *ReportFile;
};

const unsigned Environment::QueueSize = 10000;

static Environment Global;

extern "C" void OnlineBeforeFork() {
  assert(Global.ReportFile);
  fclose(Global.ReportFile);
}

extern "C" void OnlineAfterFork(int Result) {
  if (Result == 0) {
    // child process
    Global.ReportFile = fopen(Environment::GetReportFileName().c_str(), "wb");
  } else {
    // parent process
    Global.ReportFile = fopen(Environment::GetReportFileName().c_str(), "ab");
  }
}

extern "C" void ReportMissingAlias(unsigned VIDOfP, unsigned VIDOfQ, void *V) {
  pthread_mutex_lock(&Global.ReportLock);
  fprintf(Global.ReportFile, "Missing alias:\n[%u]\n[%u]\n", VIDOfP, VIDOfQ);
  pthread_mutex_unlock(&Global.ReportLock);
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

typedef void (*FreeFuncType)(void *Arg);

static void DelayedFree(void *Item, queue<void*> &Queue, FreeFuncType Free) {
  pthread_spin_lock(&Global.FreeLock);
  Queue.push(Item);
  if (Queue.size() > Environment::QueueSize) {
    Free(Queue.front());
    Queue.pop();
  }
  pthread_spin_unlock(&Global.FreeLock);
}

extern "C" void ng_free(void *MemBlock) {
  DelayedFree(MemBlock, Global.FreeQueue, free);
}

extern "C" void _ZdlPv(void *MemBlock);

extern "C" void ng_delete(void *MemBlock) {
  DelayedFree(MemBlock, Global.DeleteQueue, _ZdlPv);
}

extern "C" void _ZdaPv(void *Array);

extern "C" void ng_delete_array(void *Array) {
  DelayedFree(Array, Global.DeleteArrayQueue, _ZdaPv);
}
