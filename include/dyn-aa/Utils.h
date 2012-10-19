// Author: Jingyue

#ifndef __DYN_AA_UTILS_H
#define __DYN_AA_UTILS_H

#include <stdint.h>

#include <llvm/Value.h>

namespace dyn_aa {
struct DynAAUtils {
  static void PrintProgressBar(uint64_t Old, uint64_t Now, uint64_t Total);
  static bool PointerIsAccessed(const llvm::Value *V);
};
}

#endif
