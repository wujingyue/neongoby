// Author: Jingyue

#ifndef __DYN_AA_UTILS_H
#define __DYN_AA_UTILS_H

#include <stdint.h>
#include <string>

#include "llvm/Value.h"
#include "llvm/Pass.h"

namespace dyn_aa {
struct DynAAUtils {
  static const std::string MemAllocHookName;
  static const std::string MainArgsAllocHookName;
  static const std::string TopLevelHookName;
  static const std::string AddrTakenHookName;
  static const std::string GlobalsAllocHookName;
  static const std::string MemHooksIniterName;
  static const std::string AfterForkHookName;
  static const std::string BeforeForkHookName;
  static const std::string SlotsName;

  static void PrintProgressBar(uint64_t Old, uint64_t Now, uint64_t Total);
  static bool PointerIsDereferenced(const llvm::Value *V);
  static void PrintValue(llvm::raw_ostream &O, const llvm::Value *V);
};
}

#endif
