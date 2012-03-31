/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "instrument-memory"

#include <string>
using namespace std;

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace dyn_aa {
struct MemoryInstrumenter: public FunctionPass {
  static const string MemAllocHookName;
  static const string MemFreeHookName;
  static const string MemAccessHookName;

  static char ID;

  MemoryInstrumenter(): FunctionPass(ID) {
    MemAllocHook = MemFreeHook = NULL;
    CharType = LongType = NULL;
    CharStarType = NULL;
    VoidType = NULL;
  }
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);

 private:
  bool isMemoryAllocator(Function *F) const;
  bool isMemoryFreer(Function *F) const;
  void instrumentMemoryAllocator(const CallSite &CS);
  void instrumentMemoryFreer(const CallSite &CS);
  void checkFeatures(Module &M);

  Function *MemAllocHook, *MemFreeHook, *MemAccessHook;
  const IntegerType *CharType, *LongType;
  const PointerType *CharStarType;
  const Type *VoidType;
  vector<string> MemAllocatorNames, MemFreerNames;
};
}
using namespace dyn_aa;

char MemoryInstrumenter::ID = 0;
const string MemoryInstrumenter::MemAllocHookName = "HookMemAlloc";
const string MemoryInstrumenter::MemFreeHookName = "HookMemFree";
const string MemoryInstrumenter::MemAccessHookName = "HookMemAccess";

static RegisterPass<MemoryInstrumenter> X("instrument-memory",
                                          "Instrument memory operations",
                                          false, false);

bool MemoryInstrumenter::isMemoryAllocator(Function *F) const {
  vector<string>::const_iterator Pos = find(MemAllocatorNames.begin(),
                                            MemAllocatorNames.end(),
                                            F->getName());
  return Pos != MemAllocatorNames.end();
}

bool MemoryInstrumenter::isMemoryFreer(Function *F) const {
  vector<string>::const_iterator Pos = find(MemFreerNames.begin(),
                                            MemFreerNames.end(),
                                            F->getName());
  return Pos != MemFreerNames.end();
}

// TODO: Handle AllocaInst. 
void MemoryInstrumenter::instrumentMemoryAllocator(const CallSite &CS) {
  Function *Callee = CS.getCalledFunction();
  assert(isMemoryAllocator(Callee));
  
  // Calculate where to insert.
  Instruction *Ins = CS.getInstruction();
  BasicBlock::iterator Loc;
  if (!Ins->isTerminator()) {
    Loc = Ins;
    ++Loc;
  } else {
    assert(isa<InvokeInst>(Ins));
    Loc = cast<InvokeInst>(Ins)->getNormalDest()->begin();
  }

  // Retrive the allocated size. 
  StringRef CalleeName = Callee->getName();
  Value *Size = NULL;
  if (CalleeName == "malloc" || CalleeName == "valloc" ||
      CalleeName.startswith("_Zn")) {
    Size = CS.getArgument(0);
  } else if (CalleeName == "calloc") {
    // calloc() takes two size_t, i.e. i64. 
    // Therefore, no need to worry Mul will have two operands with different
    // types. Also, Size will always be of type i64. 
    Size = BinaryOperator::Create(Instruction::Mul,
                                  CS.getArgument(0),
                                  CS.getArgument(1),
                                  "",
                                  Loc);
  } else if (CalleeName == "memalign") {
    Size = CS.getArgument(1);
  } else if (CalleeName == "realloc") {
    assert(false && "Not supported");
  }
  assert(Size);
  
  // start = malloc(size)
  // =>
  // start = malloc(size)
  // HookMemAlloc(start, size)
  vector<Value *> Args;
  Args.push_back(Ins);
  Args.push_back(Size);
  CallInst::Create(MemAllocHook, Args.begin(), Args.end(), "", Loc);
}

void MemoryInstrumenter::instrumentMemoryFreer(const CallSite &CS) {
  Instruction *Loc = CS.getInstruction();
  Function *Callee = CS.getCalledFunction();
  assert(isMemoryFreer(Callee));

  // hook_mem_free(i8 *)
  StringRef CalleeName = Callee->getName();
  if (CalleeName == "free" || CalleeName.startswith("_Zd")) {
    CallInst::Create(MemFreeHook, CS.getArgument(0), "", Loc);
  } else {
    assert(false);
  }
}

void MemoryInstrumenter::checkFeatures(Module &M) {
  // Check whether any memory allocate or memory free functions can
  // potentially be pointed by function pointers. 
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (isMemoryAllocator(F) || isMemoryFreer(F)) {
      for (Value::use_iterator UI = F->use_begin(); UI != F->use_end(); ++UI) {
        User *Usr = *UI;
        assert(isa<CallInst>(Usr) || isa<InvokeInst>(Usr));
        CallSite CS(cast<Instruction>(Usr));
        for (unsigned i = 0; i < CS.arg_size(); ++i)
          assert(CS.getArgument(i) != F);
      }
    }
  }
  // Check whether memory allocation functions are captured. 
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (F->hasFnAttr(Attribute::NoAlias)) {
      assert(isMemoryAllocator(F));
    }
  }
}

bool MemoryInstrumenter::doInitialization(Module &M) {
  // Check whether there are unsupported language features.
  checkFeatures(M);

  // No existing functions have the same name. 
  assert(M.getFunction(MemAllocHookName) == NULL);
  assert(M.getFunction(MemFreeHookName) == NULL);

  // Setup scalar types.
  VoidType = Type::getVoidTy(M.getContext());
  CharType = IntegerType::get(M.getContext(), 8);
  CharStarType = PointerType::getUnqual(CharType);
  LongType = IntegerType::get(M.getContext(), __WORDSIZE);

  // Setup hook functions. 
  // Setup MemAllocHook
  vector<const Type *> ArgTypes;
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(LongType);
  FunctionType *MemAllocHookType = FunctionType::get(VoidType,
                                                     ArgTypes,
                                                     false);
  MemAllocHook = Function::Create(MemAllocHookType,
                                  GlobalValue::ExternalLinkage,
                                  MemAllocHookName,
                                  &M);
  
  // Setup MemFreeHook
  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  FunctionType *MemFreeHookType = FunctionType::get(VoidType, ArgTypes, false);
  MemFreeHook = Function::Create(MemFreeHookType,
                                 GlobalValue::ExternalLinkage,
                                 MemFreeHookName,
                                 &M);
  
  // Setup MemAccessHook
  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  FunctionType *MemAccessHookType = FunctionType::get(VoidType,
                                                      ArgTypes,
                                                      false);
  MemAccessHook = Function::Create(MemAccessHookType,
                                   GlobalValue::ExternalLinkage,
                                   MemAccessHookName,
                                   &M);

  // Initialize the list of memory allocatores.
  MemAllocatorNames.push_back("malloc");
  MemAllocatorNames.push_back("calloc");
  MemAllocatorNames.push_back("valloc");
  MemAllocatorNames.push_back("realloc");
  MemAllocatorNames.push_back("memalign");
  MemAllocatorNames.push_back("_Znwm");
  MemAllocatorNames.push_back("_Znaj");
  MemAllocatorNames.push_back("_Znam");

  // Initialize the list of memory freers. 
  MemFreerNames.push_back("free");
  MemFreerNames.push_back("_ZdlPv");
  MemFreerNames.push_back("_ZdaPv");

  return true;
}

bool MemoryInstrumenter::runOnFunction(Function &F) {
  for (Function::iterator BB = F.begin(); BB != F.end(); ++BB) {
    for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
      // Instrument memory allocations and releases. 
      CallSite CS(I);
      if (CS.getInstruction()) {
        // TODO: A function pointer can possibly point to memory allocation
        // or memroy free functions. We don't handle this case for now. 
        Function *Callee = CS.getCalledFunction();
        if (isMemoryAllocator(Callee)) {
          instrumentMemoryAllocator(CS);
        } else if (isMemoryFreer(Callee)) {
          instrumentMemoryFreer(CS);
        }
      }
      // Instrument pointer stores, i.e. store X *, X **. 
      // store long, long * is considered as a pointer store as well. 
      if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *ValueStored = SI->getValueOperand();
        const Type *ValueType = ValueStored->getType();
        if (ValueType == LongType || isa<PointerType>(ValueType)) {
          vector<Value *> Args;
          if (ValueType == LongType)
            Args.push_back(new IntToPtrInst(ValueStored, CharStarType, "", I));
          else
            Args.push_back(new BitCastInst(ValueStored, CharStarType, "", I));
          Args.push_back(new BitCastInst(SI->getPointerOperand(),
                                         CharStarType,
                                         "",
                                         I));
          CallInst::Create(MemAccessHook, Args.begin(), Args.end(), "", I);
        }
      }
    }
  }
  return true;
}
