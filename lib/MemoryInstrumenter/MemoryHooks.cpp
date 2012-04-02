/**
 * Author: Jingyue
 */

#include <cstdio>
#include <vector>
using namespace std;

#include <boost/unordered_map.hpp>
using namespace boost;

typedef pair<void *, int> MemNode;

unordered_map<void *, unsigned> MemVersion;

extern "C" void HookMemAlloc(void *Start, unsigned long Bound) {
  for (unsigned long i = 0; i < Bound; ++i)
    ++MemVersion[(void *)((unsigned long)Start + i)];
}

extern "C" void HookMemAccess(void *Value, void *Pointer) {
  assert(MemVersion.count(Pointer));
  if (MemVersion.count(Value)) {
    fprintf(stderr, "(%p, %u) => (%p, %u)\n",
            Pointer, MemVersion[Pointer],
            Value, MemVersion[Value]);
  }
}
