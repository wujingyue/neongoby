// Author: Jingyue

// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <cstdio>
#include <cstdlib>

#define DISABLE_REPORT

extern "C" void ReportMissingAlias(unsigned VIDOfP, unsigned VIDOfQ, void *V) {
#ifndef DISABLE_REPORT
  fprintf(stderr, "value(%u) = value(%u) = %p\n", VIDOfP, VIDOfQ, V);
#endif
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
