// Author: Jingyue

#ifndef __DYN_AA_INTERVAL_TREE_H
#define __DYN_AA_INTERVAL_TREE_H

#include <map>

#include "llvm/Value.h"

namespace dyn_aa {
// Address range. 
typedef pair<unsigned long, unsigned long> Interval;

// All intervals in the interval tree are disjoint. 
// This IntervalComparer treats overlapping intervals as equal. 
struct IntervalComparer {
  bool operator()(const Interval &I1, const Interval &I2) const {
    return I1.second <= I2.first;
  }
};

typedef std::map<Interval, llvm::Value *, IntervalComparer> IntervalTree;
}

#endif
