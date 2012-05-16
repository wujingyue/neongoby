// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <string>

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "common/IntraReach.h"
#include "common/IDAssigner.h"

using namespace std;
using namespace llvm;
using namespace rcs;

namespace dyn_aa {
struct AliasCheckerInstrumenter: public FunctionPass {
  static const string AssertNoAliasHookName;

  static char ID;

  AliasCheckerInstrumenter();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);

 private:
  bool isFree(Function *F) const;
  static bool Reachable(Instruction *P, Instruction *Q,
                        const ConstBBSet &ReachableFromP);
  void addAliasChecker(Instruction *P, Instruction *Q);

  Function *AssertNoAliasHook;
  // Types.
  Type *VoidType;
  IntegerType *CharType, *IntType;
  PointerType *CharStarType;
  // List of freers.
  vector<string> FreeNames;
};
}

using namespace dyn_aa;

static RegisterPass<AliasCheckerInstrumenter> X(
    "instrument-alias-checker",
    "Instrument the alias checker",
    false, // Is CFG Only?
    false); // Is Analysis?

const string AliasCheckerInstrumenter::AssertNoAliasHookName = "AssertNoAlias";

char AliasCheckerInstrumenter::ID = 0;

AliasCheckerInstrumenter::AliasCheckerInstrumenter(): FunctionPass(ID) {
  AssertNoAliasHook = NULL;
  VoidType = NULL;
  CharType = IntType = NULL;
  CharStarType = NULL;
}

void AliasCheckerInstrumenter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AliasAnalysis>();
  AU.addRequired<IntraReach>();
  AU.addRequired<DominatorTree>();
  AU.addRequired<IDAssigner>();
}

bool AliasCheckerInstrumenter::runOnFunction(Function &F) {
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  IntraReach &IR = getAnalysis<IntraReach>();

  // TODO: consider arguments
  InstList PointerInsts;
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
      if (Ins->getType()->isPointerTy())
        PointerInsts.push_back(Ins);
    }
  }

  vector<InstPair> ToBeChecked;
  for (size_t i = 0; i < PointerInsts.size(); ++i) {
    Instruction *P = PointerInsts[i];
    ConstBBSet ReachableBBs;
    IR.floodfill(P->getParent(), ConstBBSet(), ReachableBBs);
    for (size_t j = 0; j < PointerInsts.size(); ++j) {
      Instruction *Q = PointerInsts[j];
      if (AA.alias(P, Q) == AliasAnalysis::NoAlias &&
          Reachable(P, Q, ReachableBBs)) {
        ToBeChecked.push_back(make_pair(P, Q));
      }
    }
  }

  for (size_t i = 0; i < ToBeChecked.size(); ++i)
    addAliasChecker(ToBeChecked[i].first, ToBeChecked[i].second);

  errs() << "Added " << ToBeChecked.size() << " AssertNoAlias calls "
      << "in function " << F.getName() << "\n";

  // Remove deallocators.
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ) {
      BasicBlock::iterator NextIns = Ins; ++NextIns;
      CallSite CS(Ins);
      if (CS && isFree(CS.getCalledFunction()))
        Ins->eraseFromParent();
      Ins = NextIns;
    }
  }

  return true;
}

bool AliasCheckerInstrumenter::Reachable(Instruction *P, Instruction *Q,
                                         const ConstBBSet &ReachableFromP) {
  BasicBlock *BBOfP = P->getParent(), *BBOfQ = Q->getParent();
  
  if (BBOfP == BBOfQ) {
    // If <P> and <Q> happen to be in the same BB, returns whether
    // <P> is strictly before <Q>.
    BasicBlock::iterator I = P;
    for (++I; I != BBOfP->end(); ++I) {
      if (Q == I)
        return true;
    }
    return false;
  }

  return ReachableFromP.count(BBOfQ);
}

bool AliasCheckerInstrumenter::doInitialization(Module &M) {
  // Initialize freer list.
  FreeNames.push_back("free");
  FreeNames.push_back("_ZdlPv");
  FreeNames.push_back("_ZdaPv");

  // Initialize basic types.
  VoidType = Type::getVoidTy(M.getContext());
  CharType = Type::getInt8Ty(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());
  CharStarType = PointerType::getUnqual(CharType);

  // Initialize function types.
  vector<Type *> ArgTypes;
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  FunctionType *AssertNoAliasHookType = FunctionType::get(VoidType,
                                                          ArgTypes,
                                                          false);

  // Initialize hooks.
  AssertNoAliasHook = Function::Create(AssertNoAliasHookType,
                                       GlobalValue::ExternalLinkage,
                                       AssertNoAliasHookName,
                                       &M);

  return true;
}

void AliasCheckerInstrumenter::addAliasChecker(Instruction *P, Instruction *Q) {
  // It's safe to use DominatorTree here, because SSAUpdater preserves CFG.
  DominatorTree &DT = getAnalysis<DominatorTree>();
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  // Compute the location to add the checker.
  BasicBlock::iterator Loc = Q;
  if (isa<PHINode>(Loc))
    Loc = Loc->getParent()->getFirstNonPHI();
  else if (!Loc->isTerminator())
    ++Loc;
  else
    Loc = cast<InvokeInst>(Loc)->getNormalDest()->getFirstNonPHI();

  // Convert <P> and <Q> to "char *".
  BitCastInst *P2 = new BitCastInst(P, CharStarType, "", Loc);
  BitCastInst *Q2 = new BitCastInst(Q, CharStarType, "", Loc);

  // Compute <P> and <Q>'s value IDs.
  unsigned VIDOfP = IDA.getValueID(P), VIDOfQ = IDA.getValueID(Q);
  assert(VIDOfP != IDAssigner::INVALID_ID && VIDOfQ != IDAssigner::INVALID_ID);

  // Add a function call to AssertNoAlias.
  vector<Value *> Args;
  Args.push_back(P2);
  Args.push_back(ConstantInt::get(IntType, VIDOfP));
  Args.push_back(Q2);
  Args.push_back(ConstantInt::get(IntType, VIDOfQ));
  CallInst::Create(AssertNoAliasHook, Args, "", Loc);

  // The function call just added may be broken, because <P> may not
  // dominate <Q>. Use SSAUpdater to fix it if necessary.
  if (!DT.dominates(P, P2)) {
    SSAUpdater SU;
    PointerType *TypeOfP = cast<PointerType>(P->getType());
    SU.Initialize(TypeOfP, P->getName());
    SU.AddAvailableValue(P->getParent(), P);
    if (P->getParent() != P->getParent()->getParent()->begin()) {
      SU.AddAvailableValue(P->getParent()->getParent()->begin(),
                           ConstantPointerNull::get(TypeOfP));
    }
    assert(P2->getOperand(0) == P);
    SU.RewriteUse(P2->getOperandUse(0));
  }
}

bool AliasCheckerInstrumenter::isFree(Function *F) const {
  if (!F)
    return false;

  vector<string>::const_iterator Pos = find(FreeNames.begin(),
                                            FreeNames.end(),
                                            F->getName());
  return Pos != FreeNames.end();
}
