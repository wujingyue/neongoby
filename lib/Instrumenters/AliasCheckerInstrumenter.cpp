// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <string>

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "rcs/IntraReach.h"
#include "rcs/IDAssigner.h"

#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

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
  // Returns the number of alias checkers added.
  void addAliasCheckers(Instruction *P, const InstList &Qs);

  Function *AssertNoAliasHook;
  // Types.
  Type *VoidType;
  IntegerType *CharType, *IntType;
  PointerType *CharStarType;
  // List of freers.
  vector<string> FreeNames;
};
}

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
  // IDAssigner &IDA = getAnalysis<IDAssigner>();

#if 1
  if (F.getName() != "_Z10MYSQLparsePv")
    return false;
#endif

  errs() << "Processing function " << F.getName() << "...\n";

  // TODO: consider arguments

#if 0
  InstList PointerInsts;
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
      if (!Ins->getType()->isPointerTy())
        continue;
      if (!DynAAUtils::PointerIsAccessed(Ins))
        continue;
      PointerInsts.push_back(Ins);
    }
  }
#endif

#if 1
  // Do not query AA on modified bc.
  vector<InstPair> ToBeChecked;
  for (Function::iterator B1 = F.begin(); B1 != F.end(); ++B1) {
    ConstBBSet ReachableBBs;
    IR.floodfill(B1, ConstBBSet(), ReachableBBs);
    // B1 should be able to itself.
    assert(ReachableBBs.count(B1));
    for (BasicBlock::iterator I1 = B1->begin(); I1 != B1->end(); ++I1) {
      if (!I1->getType()->isPointerTy())
        continue;
      if (!DynAAUtils::PointerIsAccessed(I1))
        continue;
      for (ConstBBSet::iterator J = ReachableBBs.begin();
           J != ReachableBBs.end(); ++J) {
        BasicBlock *B2 = const_cast<BasicBlock *>(*J);
        BasicBlock::iterator I2 = B2->begin();
        if (B2 == B1) {
          I2 = I1;
          ++I2;
        }
        for (; I2 != B2->end(); ++I2) {
          if (!I2->getType()->isPointerTy())
            continue;
          if (!DynAAUtils::PointerIsAccessed(I2))
            continue;
          if (AA.alias(I1, I2) == AliasAnalysis::NoAlias) {
            ToBeChecked.push_back(make_pair(I1, I2));
          }
        }
      }
    }
  }
#endif

#if 0
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
#endif
  
  errs() << "Adding " << ToBeChecked.size() << " alias checkers.\n";
  for (size_t i = 0; i < ToBeChecked.size(); ) {
    InstList Qs;
    size_t j = i;
    while (j < ToBeChecked.size() &&
           ToBeChecked[j].first == ToBeChecked[i].first) {
      Qs.push_back(ToBeChecked[j].second);
      ++j;
    }
    addAliasCheckers(ToBeChecked[i].first, Qs);
    i = j;
  }
  

#if 0
  unsigned NumAliasCheckers = 0;
  for (Function::iterator B1 = F.begin(); B1 != F.end(); ++B1) {
    ConstBBSet ReachableBBs;
    IR.floodfill(B1, ConstBBSet(), ReachableBBs);
    // B1 should be able to itself.
    assert(ReachableBBs.count(B1));
    for (BasicBlock::iterator I1 = B1->begin(); I1 != B1->end(); ++I1) {
      if (!I1->getType()->isPointerTy())
        continue;
      if (IDA.getValueID(I1) == IDAssigner::INVALID_ID)
        continue;
      InstList ToBeChecked;
      for (ConstBBSet::iterator J = ReachableBBs.begin();
           J != ReachableBBs.end(); ++J) {
        BasicBlock *B2 = const_cast<BasicBlock *>(*J);
        BasicBlock::iterator I2 = B2->begin();
        if (B2 == B1) {
          I2 = I1;
          ++I2;
        }
        for (; I2 != B2->end(); ++I2) {
          if (IDA.getValueID(I2) == IDAssigner::INVALID_ID)
            continue;
          if (!I2->getType()->isPointerTy())
            continue;
          if (AA.alias(I1, I2) == AliasAnalysis::NoAlias) {
            ToBeChecked.push_back(I2);
          }
        }
      }
      addAliasCheckers(I1, ToBeChecked);
      NumAliasCheckers += ToBeChecked.size();
    }
  }
  errs() << "Added " << NumAliasCheckers << " alias checkers.\n";
#endif

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

void AliasCheckerInstrumenter::addAliasCheckers(Instruction *P,
                                                const InstList &Qs) {
  // It's safe to use DominatorTree here, because SSAUpdater preserves CFG.
  DominatorTree &DT = getAnalysis<DominatorTree>();
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  SSAUpdater SU;
  PointerType *TypeOfP = cast<PointerType>(P->getType());
  SU.Initialize(TypeOfP, P->getName());
  SU.AddAvailableValue(P->getParent(), P);
  if (P->getParent() != P->getParent()->getParent()->begin()) {
    SU.AddAvailableValue(P->getParent()->getParent()->begin(),
                         ConstantPointerNull::get(TypeOfP));
  }

  for (size_t i = 0; i < Qs.size(); ++i) {
    Instruction *Q = Qs[i];

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
      assert(P2->getOperand(0) == P);
      SU.RewriteUse(P2->getOperandUse(0));
    }
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
