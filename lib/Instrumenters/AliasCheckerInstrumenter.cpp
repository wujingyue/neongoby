// vim: sw=2

#define DEBUG_TYPE "dyn-aa"

#include <cstdio>
#include <string>

#include "llvm/IntrinsicInst.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "rcs/IDAssigner.h"
#include "rcs/IntraReach.h"

#include "dyn-aa/Utils.h"
#include "dyn-aa/BaselineAliasAnalysis.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

namespace neongoby {
struct AliasCheckerInstrumenter: public FunctionPass {
  static char ID;

  AliasCheckerInstrumenter();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);
  virtual bool doFinalization(Module &M);

 private:
  void computeAliasChecks(Function &F,
                          InstList &Pointers,
                          vector<InstPair> &Checks);
  void addAliasChecks(Function &F,
                      const InstList &Pointers,
                      const vector<InstPair> &Checks);
  void addAliasChecks(BasicBlock *BB,
                      const InstList &Pointers,
                      const vector<InstPair> &Checks,
                      AllocaInst *Slots,
                      const DenseMap<Instruction *, unsigned> &SlotOfPointer);
  unsigned addAliasChecks(Instruction *P, const InstList &Qs);
  void addAliasCheck(Instruction *P, Instruction *Q, SSAUpdater &SU);
  void instrumentFork(CallSite CS);

  Function *AliasCheck, *OnlineBeforeFork, *OnlineAfterFork;
  // Types.
  Type *VoidType;
  IntegerType *CharType, *IntType, *LongType;
  PointerType *CharStarType;
  // Input alias checks.
  DenseMap<unsigned, vector<pair<unsigned, unsigned> > > InputAliasChecks;
};
}

static RegisterPass<AliasCheckerInstrumenter> X(
    "instrument-alias-checker",
    "Instrument the alias checker",
    false, // Is CFG Only?
    false); // Is Analysis?

enum ActionOnMissingAlias {
  Abort,
  Report,
  Silence
};

static cl::opt<bool> NoPHI("no-phi",
                           cl::desc("Store pointer values into slots and "
                                    "load them at the end of functions"));
static cl::opt<string> OutputAliasChecksName("output-alias-checks",
                                             cl::desc("Dump all alias checks"));
static cl::opt<string> InputAliasChecksName("input-alias-checks",
                                            cl::desc("Read all alias checks"));
static cl::opt<ActionOnMissingAlias> ActionIfMissed(
    "action",
    cl::desc("Choose the action on missing aliases"),
    cl::values(
        clEnumValN(Abort, "abort", "abort the program"),
        clEnumValN(Report, "report", "print out the missing alias"),
        clEnumValN(Silence, "silence", "do nothing"),
        clEnumValEnd),
    cl::init(Silence));
static cl::opt<bool> CheckAllPointers("check-all-pointers-online",
                                      cl::desc("Check all pointers"));

static cl::list<string> OnlineBlackList("online-black-list",
    cl::desc("Don't add checks to these functions"));

STATISTIC(NumAliasQueries, "Number of alias queries");
STATISTIC(NumAliasChecks, "Number of alias checks");
STATISTIC(NumSSARewrites, "Number of SSA rewrites");

char AliasCheckerInstrumenter::ID = 0;

AliasCheckerInstrumenter::AliasCheckerInstrumenter(): FunctionPass(ID) {
  AliasCheck = NULL;
  VoidType = NULL;
  CharType = IntType = LongType = NULL;
  CharStarType = NULL;
}

void AliasCheckerInstrumenter::getAnalysisUsage(AnalysisUsage &AU) const {
  if (InputAliasChecksName == "") {
    // We need not run any AA if the alias checks are inputed by the user.
    AU.addRequired<AliasAnalysis>();
    AU.addRequired<BaselineAliasAnalysis>();
  }
  AU.addRequired<IntraReach>();
  AU.addRequired<DominatorTree>();
  AU.addRequired<IDAssigner>();

  // Preserve IDAssigner, so value IDs would be the same for the
  // MemoryInstruenter. Used in hybrid mode.
  AU.addPreserved<IDAssigner>();
}

void AliasCheckerInstrumenter::computeAliasChecks(Function &F,
                                                  InstList &Pointers,
                                                  vector<InstPair> &Checks) {
  IntraReach &IR = getAnalysis<IntraReach>();
  DenseMap<BasicBlock *, InstList> PointersInBB;
  // TODO: consider arguments
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
      if (!Ins->getType()->isPointerTy())
        continue;
      if (!CheckAllPointers && !DynAAUtils::PointerIsDereferenced(Ins))
        continue;
      PointersInBB[BB].push_back(Ins);
      Pointers.push_back(Ins);
    }
  }

  if (InputAliasChecksName != "") {
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    DenseMap<unsigned, vector<pair<unsigned, unsigned> > >::iterator I =
        InputAliasChecks.find(IDA.getFunctionID(&F));
    if (I != InputAliasChecks.end()) {
      for (size_t j = 0; j < I->second.size(); ++j) {
        unsigned InsID1 = I->second[j].first;
        unsigned InsID2 = I->second[j].second;
        Instruction *I1 = IDA.getInstruction(InsID1);
        Instruction *I2 = IDA.getInstruction(InsID2);
        assert(I1->getParent()->getParent() == &F &&
               I2->getParent()->getParent() == &F);
        Checks.push_back(make_pair(I1, I2));
      }
    }
    return;
  }

  // Do not query AA on modified bc. Therefore, we store the checks we are
  // going to add in Checks, and add them to the program later.
  AliasAnalysis &AA = getAnalysis<AliasAnalysis>();
  AliasAnalysis &BaselineAA = getAnalysis<BaselineAliasAnalysis>();

  unsigned NumAliasQueriesInF = 0;
  for (Function::iterator B1 = F.begin(); B1 != F.end(); ++B1) {
    if (!PointersInBB.count(B1))
      continue;
    ConstBBSet ReachableBBs;
    IR.floodfill(B1, ConstBBSet(), ReachableBBs);
    assert(ReachableBBs.count(B1));
    InstList &PointersInB1 = PointersInBB[B1];
    for (size_t i1 = 0, e1 = PointersInB1.size(); i1 < e1; ++i1) {
      Instruction *I1 = PointersInB1[i1];
      for (ConstBBSet::iterator IB2 = ReachableBBs.begin();
           IB2 != ReachableBBs.end(); ++IB2) {
        BasicBlock *B2 = const_cast<BasicBlock *>(*IB2);
        if (!PointersInBB.count(B2))
          continue;
        InstList &PointersInB2 = PointersInBB[B2];
        for (size_t i2 = (B2 == B1 ? i1 + 1 : 0), e2 = PointersInB2.size();
             i2 < e2; ++i2) {
          Instruction *I2 = PointersInB2[i2];
          ++NumAliasQueriesInF;
          if (BaselineAA.alias(I1, I2) != AliasAnalysis::NoAlias &&
              AA.alias(I1, I2) == AliasAnalysis::NoAlias) {
            // Add a check when the baseline AA says "may" and the checked AA
            // says "no".
            Checks.push_back(make_pair(I1, I2));
          }
        }
      }
    }
  }
  dbgs() << "  Issued " << NumAliasQueriesInF << " alias queries\n";
  NumAliasQueries += NumAliasQueriesInF;
}

void AliasCheckerInstrumenter::addAliasChecks(
    BasicBlock *BB,
    const InstList &Pointers,
    const vector<InstPair> &Checks,
    AllocaInst *Slots,
    const DenseMap<Instruction *, unsigned> &SlotOfPointer) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  IntraReach &IR = getAnalysis<IntraReach>();

  ConstBBSet ReachableBBs; // BBs that can reach BB
  IR.floodfill_r(BB, ConstBBSet(), ReachableBBs);

  InstList MergedPointers;
  for (size_t i = 0; i < Pointers.size(); ++i) {
    Instruction *P = Pointers[i];
    // If P does not reach BB, the alias check does not make sense because P
    // has an undefined value.
    if (!ReachableBBs.count(P->getParent())) {
      MergedPointers.push_back(NULL);
      continue;
    }

    assert(SlotOfPointer.count(P));
    unsigned SlotID = SlotOfPointer.lookup(P);
    GetElementPtrInst *Slot = GetElementPtrInst::Create(
        Slots,
        ConstantInt::get(IntType, SlotID),
        "slot",
        BB->getTerminator());
    LoadInst *MergedPointer = new LoadInst(Slot, "", BB->getTerminator());
    MergedPointers.push_back(MergedPointer);
  }

  for (size_t i = 0; i < Checks.size(); ++i) {
    Instruction *P = Checks[i].first, *Q = Checks[i].second;
    assert(SlotOfPointer.count(P) && SlotOfPointer.count(Q));
    Instruction *P2 = MergedPointers[SlotOfPointer.lookup(P)];
    Instruction *Q2 = MergedPointers[SlotOfPointer.lookup(Q)];
    if (!P2 || !Q2) {
      // Skip the check if P or Q does not reach
      // BB, because their values
      // would be undefined.
      continue;
    }

    unsigned VIDOfP = IDA.getValueID(P), VIDOfQ = IDA.getValueID(Q);
    assert(VIDOfP != IDAssigner::InvalidID && VIDOfQ != IDAssigner::InvalidID);

    vector<Value *> Args;
    Args.push_back(P2);
    Args.push_back(ConstantInt::get(IntType, VIDOfP));
    Args.push_back(Q2);
    Args.push_back(ConstantInt::get(IntType, VIDOfQ));
    CallInst::Create(AliasCheck, Args, "", BB->getTerminator());
  }
}

void AliasCheckerInstrumenter::addAliasChecks(Function &F,
                                              const InstList &Pointers,
                                              const vector<InstPair> &Checks) {
  dbgs() << "  Adding " << Checks.size() << " alias checks\n";
  NumAliasChecks += Checks.size();

  if (NoPHI) {
    BasicBlock::iterator InsertPos = F.begin()->getFirstNonPHI();
    AllocaInst *Slots = new AllocaInst(
        CharStarType,
        ConstantInt::get(IntType, Pointers.size()),
        DynAAUtils::SlotsName,
        InsertPos);
    Type *Types[] = {CharStarType, LongType};
    Function *LLVMMemset = Intrinsic::getDeclaration(F.getParent(),
                                                     Intrinsic::memset,
                                                     Types);
    vector<Value *> Args;
    Args.push_back(new BitCastInst(Slots, CharStarType, "", InsertPos));
    Args.push_back(ConstantInt::get(CharType, 0));
    Args.push_back(ConstantInt::get(LongType,
                                    sizeof(char *) * Pointers.size()));
    Args.push_back(ConstantInt::get(IntType, Slots->getAlignment()));
    Args.push_back(ConstantInt::getFalse(F.getContext()));
    CallInst::Create(LLVMMemset, Args, "", InsertPos);

    // Store each pointer to its corresponding slot.
    DenseMap<Instruction *, unsigned> SlotOfPointer;
    for (size_t i = 0; i < Pointers.size(); ++i) {
      Instruction *Ins = Pointers[i];
      SlotOfPointer[Ins] = i;

      BasicBlock::iterator Loc;
      if (InvokeInst *II = dyn_cast<InvokeInst>(Ins)) {
        Loc = II->getNormalDest()->getTerminator();
      } else {
        Loc = Ins->getParent()->getTerminator();
      }

      GetElementPtrInst *Slot = GetElementPtrInst::Create(
          Slots,
          ConstantInt::get(IntType, i),
          "slot",
          Loc);
      BitCastInst *CastSlot = new BitCastInst(
          Slot,
          PointerType::getUnqual(Ins->getType()),
          "",
          Loc);
      new StoreInst(Ins, CastSlot, Loc);
    }

    for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
      if (isa<ReturnInst>(BB->getTerminator())) {
        addAliasChecks(BB, Pointers, Checks, Slots, SlotOfPointer);
      }
    }
  } else {
    unsigned NumPHIs = 0;
    // Checks are clustered on the first item in the pair.
    for (size_t i = 0; i < Checks.size(); ) {
      InstList Qs;
      size_t j = i;
      while (j < Checks.size() &&
             Checks[j].first == Checks[i].first) {
        Qs.push_back(Checks[j].second);
        ++j;
      }
      NumPHIs += addAliasChecks(Checks[i].first, Qs);
      i = j;
    }
    dbgs() << "  Added " << NumPHIs << " phis\n";
  }
}

bool AliasCheckerInstrumenter::runOnFunction(Function &F) {
  // TODO: now the blacklist is short. if it is long, we should use a hash set
  if (OnlineBlackList.size() != 0) {
    for (unsigned i = 0; i < OnlineBlackList.size(); i++) {
      if (OnlineBlackList[i] == F.getName()) {
        return false;
      }
    }
  }

  dbgs() << "\nProcessing function " << F.getName() << "\n";
  clock_t ClockStart = clock();

  // Do not query AA on modified bc. Therefore, we store the checks we are
  // going to add in Checks, and add them to the program later.
  vector<InstPair> Checks;
  InstList Pointers;
  computeAliasChecks(F, Pointers, Checks);
  clock_t ClockFinishComputing = clock();

  if (OutputAliasChecksName != "") {
    IDAssigner &IDA = getAnalysis<IDAssigner>();
    FILE *OutputFile = fopen(OutputAliasChecksName.c_str(), "a");
    for (size_t i = 0; i < Checks.size(); ++i) {
      fprintf(OutputFile, "%u: %u %u\n",
              IDA.getFunctionID(&F),
              IDA.getInstructionID(Checks[i].first),
              IDA.getInstructionID(Checks[i].second));
    }
    fclose(OutputFile);
  }

  addAliasChecks(F, Pointers, Checks);
  clock_t ClockFinishAdding = clock();

  DEBUG(dbgs() << "Computing time = " <<
        (ClockFinishComputing - ClockStart) / CLOCKS_PER_SEC << "\n";);
  DEBUG(dbgs() << "Adding time = " <<
        (ClockFinishAdding - ClockFinishComputing) / CLOCKS_PER_SEC << "\n";);

  // Instrument forks.
  for (Function::iterator B = F.begin(); B != F.end(); ++B) {
    for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
      CallSite CS(I);
      if (CS) {
        Function *Callee = CS.getCalledFunction();
        if (Callee &&
            (Callee->getName() == "fork" || Callee->getName() == "vfork")) {
          instrumentFork(CS);
        }
      }
    }
  }

  return true;
}

void AliasCheckerInstrumenter::instrumentFork(CallSite CS) {
  Instruction *I = CS.getInstruction();
  CallInst::Create(OnlineBeforeFork, "", I);
  assert(isa<CallInst>(I));
  BasicBlock::iterator Next = I; ++Next;
  CallInst::Create(OnlineAfterFork, I, "", Next);
}

bool AliasCheckerInstrumenter::doInitialization(Module &M) {
  // Initialize basic types.
  VoidType = Type::getVoidTy(M.getContext());
  CharType = Type::getInt8Ty(M.getContext());
  IntType = Type::getInt32Ty(M.getContext());
  // FIXME: Use TargetData
  LongType = Type::getIntNTy(M.getContext(), __WORDSIZE);
  CharStarType = PointerType::getUnqual(CharType);

  // Initialize function types.
  vector<Type *> ArgTypes;
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  // XXX(void *P, unsigned IDOfP, void *Q, unsigned IDOfQ)
  FunctionType *AliasCheckType = FunctionType::get(VoidType, ArgTypes, false);
  // OnlineBeforeFork()
  FunctionType *OnlineBeforeForkType = FunctionType::get(VoidType, false);
  // OnlineAfterFork(int ResultOfFork)
  FunctionType *OnlineAfterForkType = FunctionType::get(VoidType,
                                                        IntType,
                                                        false);

  // Initialize hooks.
  string AliasCheckName;
  switch (ActionIfMissed) {
    case Abort: AliasCheckName = "AbortIfMissed"; break;
    case Report: AliasCheckName = "ReportIfMissed"; break;
    case Silence: AliasCheckName = "SilenceIfMissed"; break;
  }
  AliasCheck = Function::Create(AliasCheckType,
                                GlobalValue::ExternalLinkage,
                                AliasCheckName,
                                &M);
  AliasCheck->setDoesNotThrow(true);
  OnlineBeforeFork = Function::Create(OnlineBeforeForkType,
                                      GlobalValue::ExternalLinkage,
                                      "OnlineBeforeFork",
                                      &M);
  OnlineAfterFork = Function::Create(OnlineAfterForkType,
                                     GlobalValue::ExternalLinkage,
                                     "OnlineAfterFork",
                                     &M);

  assert(InputAliasChecksName == "" || OutputAliasChecksName == "");
  // Initialize the output file for alias checks if necessary.
  if (OutputAliasChecksName != "") {
    FILE *OutputFile = fopen(OutputAliasChecksName.c_str(), "w");
    fclose(OutputFile);
  }
  // Read input alias checks.
  if (InputAliasChecksName != "") {
    FILE *InputFile = fopen(InputAliasChecksName.c_str(), "r");
    unsigned FuncID, InsID1, InsID2;
    while (fscanf(InputFile, "%u: %u %u", &FuncID, &InsID1, &InsID2) == 3) {
      InputAliasChecks[FuncID].push_back(make_pair(InsID1, InsID2));
    }
    fclose(InputFile);
  }

  return true;
}

bool AliasCheckerInstrumenter::doFinalization(Module &M) {
  // replace free() and delete with our implementation
  vector<pair<string, string> > ReplacePairs;
  ReplacePairs.push_back(make_pair<string, string>("free", "ng_free"));
  ReplacePairs.push_back(make_pair<string, string>("_ZdlPv", "ng_delete"));
  ReplacePairs.push_back(make_pair<string, string>("_ZdaPv",
                                                   "ng_delete_array"));
  for (vector<pair<string, string> >::iterator it = ReplacePairs.begin();
      it != ReplacePairs.end(); it++) {
    Function *Func = M.getFunction(it->first);
    if (Func != NULL) {
      assert(M.getFunction(it->second) == NULL && "runtime function's name is \
          in conflict with original functions' names");
      Func->setName(it->second);
    }
  }

  return true;
}

unsigned AliasCheckerInstrumenter::addAliasChecks(Instruction *P,
                                              const InstList &Qs) {
  SmallVector<PHINode *, 8> InsertedPHIs;
  SSAUpdater SU(&InsertedPHIs);
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
  return InsertedPHIs.size();
}

void AliasCheckerInstrumenter::addAliasCheck(Instruction *P,
                                             Instruction *Q,
                                             SSAUpdater &SU) {
  // It's safe to use DominatorTree here, because SSAUpdater preserves CFG.
  DominatorTree &DT = getAnalysis<DominatorTree>();
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  if (InvokeInst *II = dyn_cast<InvokeInst>(P)) {
    //   P = invoke ... to NormalBB, unwind to UnwindBB
    // UnwindBB:
    //   ...
    //   Q = ...
    //
    // In the above situation, we do not add alias checks for P and Q, because
    // using P's value in the unwind basic block is invalid.
    //
    // Q may even be in other basic blocks dominated by UnwindBB. Filter out
    // this case as well.
    if (DT.dominates(II->getUnwindDest(), Q->getParent()))
      return;
  }

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
  CallInst::Create(AliasCheck, Args, "", Loc);

  // The function call just added may be broken, because <P> may not
  // dominate <Q>. Use SSAUpdater to fix it if necessary.
  if (!DT.dominates(P, P2)) {
    assert(P2->getOperand(0) == P);
    SU.RewriteUse(P2->getOperandUse(0));
    ++NumSSARewrites;
  }
}
