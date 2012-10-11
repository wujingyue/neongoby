// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <string>

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
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
  void computeAliasChecks(Function &F,
                          DenseMap<BasicBlock *, InstList> &PointerInsts,
                          vector<InstPair> &Checks);
  void addAliasChecks(const vector<InstPair> &Checks);
  void addAliasChecks(Instruction *P, const InstList &Qs);
  void addAliasCheck(Instruction *P, Instruction *Q, SSAUpdater &SU);

  Function *AssertNoAliasHook;
  // Types.
  Type *VoidType;
  IntegerType *CharType, *IntType;
  PointerType *CharStarType;
};
}

static RegisterPass<AliasCheckerInstrumenter> X(
    "instrument-alias-checker",
    "Instrument the alias checker",
    false, // Is CFG Only?
    false); // Is Analysis?

static cl::opt<unsigned> MaxNumAliasChecks(
    "max-alias-checks",
    cl::desc("Add at most this many alias checks. Used for debugging"),
    cl::init((unsigned)-1));

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

#if 0
static bool SortBBByName(const BasicBlock *B1, const BasicBlock *B2) {
  assert(B1->hasName() && B2->hasName());
  return B1->getName() < B2->getName();
}
#endif

void AliasCheckerInstrumenter::computeAliasChecks(
    Function &F,
    DenseMap<BasicBlock *, InstList> &PointerInsts,
    vector<InstPair> &Checks) {
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  IntraReach &IR = getAnalysis<IntraReach>();

  // TODO: consider arguments
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
      if (!Ins->getType()->isPointerTy())
        continue;
      if (!DynAAUtils::PointerIsAccessed(Ins))
        continue;
      PointerInsts[BB].push_back(Ins);
    }
  }

  // Do not query AA on modified bc. Therefore, we store the checks we are
  // going to add in Checks, and add them to the program later.
  unsigned NumAliasQueries = 0;
  for (Function::iterator B1 = F.begin(); B1 != F.end(); ++B1) {
    if (!PointerInsts.count(B1))
      continue;
    ConstBBSet ReachableBBs;
    IR.floodfill(B1, ConstBBSet(), ReachableBBs);
    assert(ReachableBBs.count(B1));
    InstList &PointerInstsInB1 = PointerInsts[B1];
    for (size_t i1 = 0, e1 = PointerInstsInB1.size(); i1 < e1; ++i1) {
      Instruction *I1 = PointerInstsInB1[i1];
      for (ConstBBSet::iterator IB2 = ReachableBBs.begin();
           IB2 != ReachableBBs.end(); ++IB2) {
        BasicBlock *B2 = const_cast<BasicBlock *>(*IB2);
        if (!PointerInsts.count(B2))
          continue;
        InstList &PointerInstsInB2 = PointerInsts[B2];
        for (size_t i2 = (B2 == B1 ? i1 + 1 : 0), e2 = PointerInstsInB2.size();
             i2 < e2; ++i2) {
          Instruction *I2 = PointerInstsInB2[i2];
          ++NumAliasQueries;
          if (AA.alias(I1, I2) == AliasAnalysis::NoAlias) {
            Checks.push_back(make_pair(I1, I2));
            if (Checks.size() == MaxNumAliasChecks)
              return;
          }
        }
      }
    }
  }
  errs() << "# of alias queries = " << NumAliasQueries << "\n";
  assert(Checks.size() <= MaxNumAliasChecks);
}

void AliasCheckerInstrumenter::addAliasChecks(const vector<InstPair> &Checks) {
  errs() << "Adding " << Checks.size() << " alias checkers...\n";
  // Checks are clustered on the first item in the pair.
  for (size_t i = 0; i < Checks.size(); ) {
    InstList Qs;
    size_t j = i;
    while (j < Checks.size() &&
           Checks[j].first == Checks[i].first) {
      Qs.push_back(Checks[j].second);
      ++j;
    }
    addAliasChecks(Checks[i].first, Qs);
    i = j;
  }
}

bool AliasCheckerInstrumenter::runOnFunction(Function &F) {
  errs() << "Processing function " << F.getName() << "...\n";

  // Do not query AA on modified bc. Therefore, we store the checks we are
  // going to add in Checks, and add them to the program later.
  DenseMap<BasicBlock *, InstList> PointerInsts;
  vector<InstPair> Checks;
  computeAliasChecks(F, PointerInsts, Checks);

  addAliasChecks(Checks);

  // Remove deallocators.
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ) {
      BasicBlock::iterator NextIns = Ins; ++NextIns;
      if (isFreeCall(Ins))
        Ins->eraseFromParent();
      Ins = NextIns;
    }
  }

  return true;
}

bool AliasCheckerInstrumenter::doInitialization(Module &M) {
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

void AliasCheckerInstrumenter::addAliasChecks(Instruction *P,
                                              const InstList &Qs) {
  SSAUpdater SU;
  PointerType *TypeOfP = cast<PointerType>(P->getType());
  SU.Initialize(TypeOfP, P->getName());
  SU.AddAvailableValue(P->getParent(), P);
  if (P->getParent() != P->getParent()->getParent()->begin()) {
    SU.AddAvailableValue(P->getParent()->getParent()->begin(),
                         ConstantPointerNull::get(TypeOfP));
  }

  for (size_t i = 0; i < Qs.size(); ++i) {
    addAliasCheck(P, Qs[i], SU);
  }
}

void AliasCheckerInstrumenter::addAliasCheck(Instruction *P,
                                             Instruction *Q,
                                             SSAUpdater &SU) {
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
  assert(VIDOfP != IDAssigner::InvalidID && VIDOfQ != IDAssigner::InvalidID);

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
