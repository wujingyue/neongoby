// Author: Jingyue

// Hook functions are declared with extern "C", because we want to disable
// the C++ name mangling and make the instrumentation easier.

#include <cstdio>
#include <cassert>

extern "C" void AssertNoAlias(void *P, unsigned VIDOfP,
                              void *Q, unsigned VIDOfQ) {
  if (P == Q && P) {
    fprintf(stderr, "Value %u and value %u alias.\n", VIDOfP, VIDOfQ);
    fprintf(stderr, "They point to the same location %p\n", P);
    assert(false);
  }
}
