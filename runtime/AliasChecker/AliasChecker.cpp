// Author: Jingyue

// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <cstdio>
#include <cstdlib>

extern "C" void ReportMissingAlias(unsigned VIDOfP, unsigned VIDOfQ, void *V) {
  fprintf(stderr, "value(%u) = value(%u) = %p\n", VIDOfP, VIDOfQ, V);
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
