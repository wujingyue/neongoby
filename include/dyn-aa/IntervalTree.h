#ifndef __DYN_AA_INTERVAL_TREE_H
#define __DYN_AA_INTERVAL_TREE_H

#include <map>

#include "llvm/Value.h"

namespace neongoby {
// Address range.
// All intervals in the interval tree are disjoint.
// The comparer treats overlapping intervals as equal.
struct Interval {
  Interval(unsigned long S, unsigned long E): Start(S), End(E) {}
  bool operator<(const Interval &Second) const {
    return End <= Second.Start;
  }

  unsigned long Start;
  unsigned long End;
};

template <typename T>
struct IntervalTree: public std::map<Interval, T> {
};
}

#endif
