// Author: Jingyue

#include <string>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/raw_ostream.h"

using namespace std;
using namespace llvm;

namespace dyn_aa {
struct AliasCheckerInliner: public ModulePass {
  static const string AssertNoAliasHookName;

  static char ID;

  AliasCheckerInliner(): ModulePass(ID) {}
  virtual bool runOnModule(Module &M);
};
}

using namespace dyn_aa;

static RegisterPass<AliasCheckerInliner> X(
    "inline-alias-checker",
    "Inline the alias checker",
    false,
    false);

const string AliasCheckerInliner::AssertNoAliasHookName = "AssertNoAlias";

char AliasCheckerInliner::ID = 0;

bool AliasCheckerInliner::runOnModule(Module &M) {
  Function *AssertNoAliasHook = M.getFunction(AssertNoAliasHookName);
  assert(AssertNoAliasHook && "Cannot find AssertNoAlias");
  InlineFunctionInfo IFI;
  unsigned NumCallSitesProcessed = 0;
  for (Value::use_iterator UI = AssertNoAliasHook->use_begin();
       UI != AssertNoAliasHook->use_end(); ++UI) {
    ++NumCallSitesProcessed;
    if (CallInst *CI = dyn_cast<CallInst>(*UI)) {
      if (CI->getCalledFunction() == AssertNoAliasHook) {
        InlineFunction(CI, IFI);
      }
    }
    if (NumCallSitesProcessed % 1000 == 0) {
      errs() << NumCallSitesProcessed << "\n";
    }
  }
  return true;
}
