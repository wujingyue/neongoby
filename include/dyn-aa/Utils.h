#ifndef __DYN_AA_UTILS_H
#define __DYN_AA_UTILS_H

#include <stdint.h>
#include <string>

#include "llvm/Value.h"
#include "llvm/Pass.h"

namespace neongoby {
struct DynAAUtils {
  static const std::string MemAllocHookName;
  static const std::string MainArgsAllocHookName;
  static const std::string TopLevelHookName;
  static const std::string EnterHookName;
  static const std::string StoreHookName;
  static const std::string CallHookName;
  static const std::string ReturnHookName;
  static const std::string GlobalsAllocHookName;
  static const std::string BasicBlockHookName;
  static const std::string MemHooksIniterName;
  static const std::string AfterForkHookName;
  static const std::string BeforeForkHookName;
  static const std::string VAStartHookName;
  static const std::string SlotsName;

  static void PrintProgressBar(uint64_t Old, uint64_t Now, uint64_t Total);
  static bool PointerIsDereferenced(const llvm::Value *V);
  static void PrintValue(llvm::raw_ostream &O, const llvm::Value *V);
  static bool IsMalloc(const llvm::Function *F);
  static bool IsMallocCall(const llvm::Value *V);
  static bool IsIntraProcQuery(const llvm::Value *V1, const llvm::Value *V2);
  static bool IsReallyIntraProcQuery(const llvm::Value *V1,
                                     const llvm::Value *V2);
  static const llvm::Function *GetContainingFunction(const llvm::Value *V);
};
}

#endif
