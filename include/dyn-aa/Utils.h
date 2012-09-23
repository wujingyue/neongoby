// Author: Jingyue

#ifndef __DYN_AA_UTILS_H
#define __DYN_AA_UTILS_H

#include <stdint.h>

namespace dyn_aa {
struct DynAAUtils {
  static void PrintProgressBar(uint64_t Finished, uint64_t Total);
};
}

#endif
