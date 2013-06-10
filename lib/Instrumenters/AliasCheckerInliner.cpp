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

namespace neongoby {
struct AliasCheckerInliner: public FunctionPass {
  static char ID;

  AliasCheckerInliner(): FunctionPass(ID) {}
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);
  virtual bool doFinalization(Module &M);

 private:
  void inlineAbortIfMissed(CallInst *CI, BasicBlock *TrapBB);
  void inlineReportOrSilenceIfMissed(CallInst *CI);
  void insertBranchTo(BasicBlock *BB, BasicBlock *ErrorBB, BasicBlock *NextBB,
                      Value *P, Value *Q);

  Function *AbortIfMissed, *ReportIfMissed, *SilenceIfMissed;
  Function *ReportMissingAlias, *SilenceMissingAlias;
};
}

using namespace neongoby;

static RegisterPass<AliasCheckerInliner> X(
    "inline-alias-checker",
    "Inline the alias checker",
    false,
    false);

char AliasCheckerInliner::ID = 0;

bool AliasCheckerInliner::doInitialization(Module &M) {
  AbortIfMissed = M.getFunction("AbortIfMissed");
  ReportIfMissed = M.getFunction("ReportIfMissed");
  SilenceIfMissed = M.getFunction("SilenceIfMissed");

  Type *VoidType = Type::getVoidTy(M.getContext());
  Type *IntType = Type::getInt32Ty(M.getContext());
  Type *CharStarType = PointerType::getUnqual(Type::getInt8Ty(M.getContext()));

  assert(M.getFunction("ReportMissingAlias") == NULL);
  assert(M.getFunction("SilenceMissingAlias") == NULL);
  vector<Type *> ArgTypes;
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  FunctionType *ReportOrSilenceMissingAliasType = FunctionType::get(VoidType,
                                                                    ArgTypes,
                                                                    false);
  if (ReportIfMissed != NULL) {
    ReportMissingAlias = Function::Create(ReportOrSilenceMissingAliasType,
                                          GlobalValue::ExternalLinkage,
                                          "ReportMissingAlias",
                                          &M);
    ReportMissingAlias->setDoesNotThrow(true);
  }
  if (SilenceIfMissed != NULL) {
    SilenceMissingAlias = Function::Create(ReportOrSilenceMissingAliasType,
                                           GlobalValue::ExternalLinkage,
                                           "SilenceMissingAlias",
                                           &M);
    SilenceMissingAlias->setDoesNotThrow(true);
  }
  return true;
}

bool AliasCheckerInliner::runOnFunction(Function &F) {
  BasicBlock *TrapBB = NULL;
  if (AbortIfMissed) {
    TrapBB = BasicBlock::Create(F.getContext(), "trap", &F);
    // TODO: Add ReportMissingAlias
    Function *LLVMTrap = Intrinsic::getDeclaration(F.getParent(),
                                                   Intrinsic::trap);
    CallInst::Create(LLVMTrap, "", TrapBB);
    new UnreachableInst(F.getContext(), TrapBB);
  }

  // We traverse BB lists and instruction lists backwards for performance
  // reasons. BasicBlock::splitBasicBlock does not take a constant time,
  // because it needs to update the parent BB of the instructions in the new
  // BB. Therefore, spliting a BB at each AssertNoAlias while traversing the BB
  // forwards would take O(n^2) time. However, doing so while traversing the BB
  // backwards only takes O(n) time, because the new basic block won't be split
  // again.
  for (Function::iterator BB = F.end(); BB != F.begin(); ) {
    --BB;
    for (BasicBlock::iterator Ins = BB->end(); Ins != BB->begin(); ) {
      --Ins;
      if (CallInst *CI = dyn_cast<CallInst>(Ins)) {
        if (Function *Callee = CI->getCalledFunction()) {
          if (Callee == AbortIfMissed) {
            inlineAbortIfMissed(CI, TrapBB);
            Ins = BB->getTerminator();
          } else if (Callee == ReportIfMissed || Callee == SilenceIfMissed) {
            inlineReportOrSilenceIfMissed(CI);
            Ins = BB->getTerminator();
          }
        }
      }
    }
  }

  return true;
}

bool AliasCheckerInliner::doFinalization(Module &M) {
  // These two functions are inlined. No use to keep their declarations.
  if (AbortIfMissed != NULL)
    AbortIfMissed->eraseFromParent();
  if (ReportIfMissed != NULL)
    ReportIfMissed->eraseFromParent();
  if (SilenceIfMissed != NULL)
    SilenceIfMissed->eraseFromParent();

  return true;
}

void AliasCheckerInliner::inlineAbortIfMissed(CallInst *CI,
                                              BasicBlock *TrapBB) {
  BasicBlock *BB = CI->getParent();
  CallSite CS(CI);
  Value *P = CS.getArgument(0), *Q = CS.getArgument(2);
  // BB:
  //   xxx
  //   call AbortIfMissed(P, VID(P), Q, VID(Q))
  //   yyy
  //
  // =>
  //
  // BB:
  //   xxx
  //   br NewBB
  // NewBB:
  //   call AbortIfMissed(P, VID(P), Q, VID(Q))
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
  BasicBlock *NewBB = BB->splitBasicBlock(CI, "bb");
  insertBranchTo(BB, TrapBB, NewBB, P, Q);
  CI->eraseFromParent();
}

void AliasCheckerInliner::inlineReportOrSilenceIfMissed(CallInst *CI) {
  BasicBlock *BB = CI->getParent();
  CallSite CS(CI);
  Value *P = CS.getArgument(0), *Q = CS.getArgument(2);
  Value *VIDOfP = CS.getArgument(1), *VIDOfQ = CS.getArgument(3);
  // BB:
  //   xxx
  //   call ReportIfMissed(P, VID(P), Q, VID(Q))
  //   yyy
  //
  // =>
  //
  // BB:
  //   xxx
  //   br NewBB
  // NewBB:
  //   call ReportIfMissed(P, VID(P), Q, VID(Q))
  //   yyy
  //
  // =>
  //
  // BB:
  //   xxx
  //   C1 = icmp eq P, Q
  //   C2 = icmp ne P, null
  //   C3 = and C1, C2
  //   br C3, ReportBB, NewBB
  // NewBB:
  //   yyy
  // ReportBB:
  //   ReportMissingAlias(VIDOfP, VIDOfQ, P)
  //   br NewBB

  // Create NewBB.
  BasicBlock *NewBB = BB->splitBasicBlock(CI, "bb");
  // Create ReportBB.
  BasicBlock *ReportBB = BasicBlock::Create(BB->getContext(),
                                            "report",
                                            BB->getParent());
  vector<Value *> Args;
  Args.push_back(VIDOfP);
  Args.push_back(VIDOfQ);
  Args.push_back(P);
  if (CI->getCalledFunction() == ReportIfMissed)
    CallInst::Create(ReportMissingAlias, Args, "", ReportBB);
  else if (CI->getCalledFunction() == SilenceIfMissed)
    CallInst::Create(SilenceMissingAlias, Args, "", ReportBB);
  else
    assert(false);
  BranchInst::Create(NewBB, ReportBB);
  // Update BB.
  insertBranchTo(BB, ReportBB, NewBB, P, Q);
  CI->eraseFromParent();
}

void AliasCheckerInliner::insertBranchTo(BasicBlock *BB,
                                         BasicBlock *ErrorBB,
                                         BasicBlock *NextBB,
                                         Value *P, Value *Q) {
  BB->getTerminator()->eraseFromParent();
  Value *C1 = new ICmpInst(*BB, CmpInst::ICMP_EQ, P, Q);
  Value *C2 = new ICmpInst(*BB, CmpInst::ICMP_NE, P,
                           ConstantPointerNull::get(
                               cast<PointerType>(P->getType())));
  Value *C3 = BinaryOperator::Create(Instruction::And, C1, C2, "", BB);
  BranchInst::Create(ErrorBB, NextBB, C3, BB);
}
