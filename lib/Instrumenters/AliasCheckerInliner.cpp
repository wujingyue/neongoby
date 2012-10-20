// Author: Jingyue

#include <string>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Constants.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CallSite.h"

using namespace std;
using namespace llvm;

namespace dyn_aa {
struct AliasCheckerInliner: public FunctionPass {
  static const string AssertNoAliasHookName;

  static char ID;

  AliasCheckerInliner(): FunctionPass(ID) {}
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);

 private:
  BasicBlock *TrapBB;
  Function *AssertNoAliasHook;
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

bool AliasCheckerInliner::doInitialization(Module &M) {
  AssertNoAliasHook = M.getFunction(AssertNoAliasHookName);
  assert(AssertNoAliasHook && "Cannot find AssertNoAlias");
  return false;
}

bool AliasCheckerInliner::runOnFunction(Function &F) {
  TrapBB = BasicBlock::Create(F.getContext(), "trap", &F);
  Function *LLVMTrap = Intrinsic::getDeclaration(F.getParent(),
                                                 Intrinsic::trap);
  CallInst::Create(LLVMTrap, "", TrapBB);
  new UnreachableInst(F.getContext(), TrapBB);

  for (Function::iterator BB = F.end(); BB != F.begin(); ) {
    --BB;
    for (BasicBlock::iterator Ins = BB->end(); Ins != BB->begin(); ) {
      --Ins;
      if (CallInst *CI = dyn_cast<CallInst>(Ins)) {
        if (CI->getCalledFunction() == AssertNoAliasHook) {
          CallSite CS(CI);
          Value *P = CS.getArgument(0), *Q = CS.getArgument(2);
          // BB:
          //   xxx
          //   call AssertNoAlias(P, VID(P), Q, VID(Q))
          //   yyy
          //
          // =>
          //
          // BB:
          //   xxx
          //   br NewBB
          // NewBB:
          //   call AssertNoAlias(P, VID(P), Q, VID(Q))
          //   yyy
          //
          // =>
          //
          // BB:
          //   xxx
          //   C1 = icmp eq P, Q
          //   C2 = icmp ne P, null
          //   C3 = and C1, C2
          //   br C3, TrapBB, NewBB
          // NewBB:
          //   yyy
          BasicBlock *NewBB = BB->splitBasicBlock(Ins, "bb");
          BB->getTerminator()->eraseFromParent();
          Value *C1 = new ICmpInst(*BB, CmpInst::ICMP_EQ, P, Q);
          Value *C2 = new ICmpInst(*BB, CmpInst::ICMP_NE, P,
                                   ConstantPointerNull::get(
                                       cast<PointerType>(P->getType())));
          Value *C3 = BinaryOperator::Create(Instruction::And, C1, C2, "", BB);
          BranchInst::Create(TrapBB, NewBB, C3, BB);
          Ins->eraseFromParent();
          Ins = BB->getTerminator();
        }
      }
    }
  }
  return true;
}
