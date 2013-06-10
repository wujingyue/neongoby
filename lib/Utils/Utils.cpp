#include <cassert>
#include <string>

#include "llvm/Argument.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include "dyn-aa/Utils.h"

using namespace std;
using namespace llvm;
using namespace neongoby;

const string DynAAUtils::MemAllocHookName = "HookMemAlloc";
const string DynAAUtils::MainArgsAllocHookName = "HookMainArgsAlloc";
const string DynAAUtils::TopLevelHookName = "HookTopLevel";
const string DynAAUtils::EnterHookName = "HookEnter";
const string DynAAUtils::StoreHookName = "HookStore";
const string DynAAUtils::CallHookName = "HookCall";
const string DynAAUtils::ReturnHookName = "HookReturn";
const string DynAAUtils::GlobalsAllocHookName = "HookGlobalsAlloc";
const string DynAAUtils::BasicBlockHookName = "HookBasicBlock";
const string DynAAUtils::MemHooksIniterName = "InitMemHooks";
const string DynAAUtils::AfterForkHookName = "HookAfterFork";
const string DynAAUtils::BeforeForkHookName = "HookBeforeFork";
const string DynAAUtils::VAStartHookName = "HookVAStart";
const string DynAAUtils::SlotsName = "ng.slots";

void DynAAUtils::PrintProgressBar(uint64_t Old, uint64_t Now, uint64_t Total) {
  assert(Total > 0);
  assert(Now <= Total);

  if (Now == 0) {
    errs().changeColor(raw_ostream::BLUE);
    errs() << " [0%]";
    errs().resetColor();
  } else {
    unsigned CurrentPercentage = Now * 10 / Total;
    unsigned OldPercentage = Old * 10 / Total;
    for (unsigned Percentage = OldPercentage + 1;
         Percentage <= CurrentPercentage; ++Percentage) {
      errs().changeColor(raw_ostream::BLUE);
      errs() << " [" << Percentage * 10 << "%]";
      errs().resetColor();
    }
  }
}

bool DynAAUtils::PointerIsDereferenced(const Value *V) {
  assert(V->getType()->isPointerTy());
  if (isa<Function>(V)) {
    // We always consider missing call edges important.
    return true;
  }
  {
    ImmutableCallSite CS(V);
    if (CS) {
      if (const Function *Callee = CS.getCalledFunction()) {
        if (Callee->isDeclaration()) {
          return true;
        }
      }
    }
  }
  for (Value::const_use_iterator UI = V->use_begin();
       UI != V->use_end(); ++UI) {
    if (const LoadInst *LI = dyn_cast<LoadInst>(*UI)) {
      if (LI->getPointerOperand() == V)
        return true;
    }
    if (const StoreInst *SI = dyn_cast<StoreInst>(*UI)) {
      if (SI->getPointerOperand() == V)
        return true;
    }
    ImmutableCallSite CS(*UI);
    if (CS) {
      if (CS.getCalledValue() == V) {
        // Return true if V is used as a callee.
        return true;
      }
      if (const Function *Callee = CS.getCalledFunction()) {
        if (Callee->isDeclaration()) {
          // Treat as deref'ed if used by an external function call.
          return true;
        }
      }
    }
  }
  return false;
}

void DynAAUtils::PrintValue(raw_ostream &O, const Value *V) {
  if (isa<Function>(V)) {
    O << V->getName();
  } else if (const Argument *Arg = dyn_cast<Argument>(V)) {
    O << Arg->getParent()->getName() << ":  " << *Arg;
  } else if (const Instruction *Ins = dyn_cast<Instruction>(V)) {
    O << Ins->getParent()->getParent()->getName() << ":";
    O << *Ins;
  } else {
    O << *V;
  }
}

bool DynAAUtils::IsMalloc(const Function *F) {
  StringRef Name = F->getName();
  return (Name == "malloc" ||
          Name == "calloc" ||
          Name == "valloc" ||
          Name == "realloc" ||
          Name == "memalign" ||
          Name == "_Znwj" ||
          Name == "_Znwm" ||
          Name == "_Znaj" ||
          Name == "_Znam" ||
          Name == "strdup" ||
          Name == "__strdup" ||
          Name == "getline");
}

bool DynAAUtils::IsMallocCall(const Value *V) {
  ImmutableCallSite CS(V);
  if (!CS)
    return false;

  const Function *Callee = CS.getCalledFunction();
  if (!Callee)
    return false;
  return IsMalloc(Callee);
}

bool DynAAUtils::IsIntraProcQuery(const Value *V1, const Value *V2) {
  assert(V1->getType()->isPointerTy() && V2->getType()->isPointerTy());
  const Function *F1 = GetContainingFunction(V1);
  const Function *F2 = GetContainingFunction(V2);
  return F1 == NULL || F2 == NULL || F1 == F2;
}

// FIXME: Oh my god! What a name!
bool DynAAUtils::IsReallyIntraProcQuery(const Value *V1, const Value *V2) {
  assert(V1->getType()->isPointerTy() && V2->getType()->isPointerTy());
  const Function *F1 = GetContainingFunction(V1);
  const Function *F2 = GetContainingFunction(V2);
  return F1 != NULL && F2 != NULL && F1 == F2;
}

const Function *DynAAUtils::GetContainingFunction(const Value *V) {
  if (const Instruction *Ins = dyn_cast<Instruction>(V))
    return Ins->getParent()->getParent();
  if (const Argument *Arg = dyn_cast<Argument>(V))
    return Arg->getParent();
  return NULL;
}
