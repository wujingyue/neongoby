// Author: Jingyue

#define DEBUG_TYPE "dyn-aa"

#include <string>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"

#include "rcs/IDAssigner.h"

using namespace llvm;
using namespace std;
using namespace rcs;

namespace dyn_aa {
struct MemoryInstrumenter: public ModulePass {
  static const string MemAllocHookName;
  static const string MainArgsAllocHookName;
  static const string TopLevelHookName;
  static const string AddrTakenHookName;
  static const string GlobalsAllocHookName;
  static const string MemHooksIniterName;

  static char ID;

  MemoryInstrumenter();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

 private:
  static uint64_t BitLengthToByteLength(uint64_t Size);
  // Includes not only "malloc", but also similar memory allocation functions
  // such as "valloc" and "calloc".
  bool isMalloc(Function *F) const;
  void instrumentInstructionIfNecessary(Instruction *I);
  // Emit code to handle memory allocation.
  // If <Success>, range [<Start>, <Start> + <Size>) is allocated.
  void instrumentMemoryAllocation(Value *Start, Value *Size, Value *Success,
                                  Instruction *Loc);
  void instrumentMalloc(const CallSite &CS);
  void instrumentAlloca(AllocaInst *AI);
  void instrumentStoreInst(StoreInst *SI);
  void instrumentPointer(Value *V, Instruction *Loc);
  void instrumentPointerInstruction(Instruction *I);
  void instrumentPointerParameters(Function *F);
  void instrumentGlobals(Module &M);
  void instrumentMainArgs(Module &M);
  void checkFeatures(Module &M);
  void setupScalarTypes(Module &M);
  void setupHooks(Module &M);
  void lowerGlobalCtors(Module &M);
  void addNewGlobalCtor(Module &M);

  Function *MemAllocHook;
  Function *MainArgsAllocHook;
  Function *TopLevelHook;
  Function *AddrTakenHook;
  Function *GlobalsAllocHook;
  Function *MemHooksIniter;
  Function *Main;
  IntegerType *CharType, *LongType, *IntType;
  PointerType *CharStarType;
  Type *VoidType;
  vector<string> MallocNames;
  DenseSet<Instruction *> AddedByUs;
};
}

using namespace dyn_aa;

char MemoryInstrumenter::ID = 0;
const string MemoryInstrumenter::MemAllocHookName = "HookMemAlloc";
const string MemoryInstrumenter::MainArgsAllocHookName = "HookMainArgsAlloc";
const string MemoryInstrumenter::TopLevelHookName = "HookTopLevel";
const string MemoryInstrumenter::AddrTakenHookName = "HookAddrTaken";
const string MemoryInstrumenter::GlobalsAllocHookName = "HookGlobalsAlloc";
const string MemoryInstrumenter::MemHooksIniterName = "InitMemHooks";

static RegisterPass<MemoryInstrumenter> X("instrument-memory",
                                          "Instrument memory operations",
                                          false, false);

void MemoryInstrumenter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetData>();
  AU.addRequired<IDAssigner>();
}

uint64_t MemoryInstrumenter::BitLengthToByteLength(uint64_t Size) {
  // Except bool, every type size must be a multipler of 8.
  assert(Size == 1 || Size % 8 == 0);
  // The type size shouldn't be a very large number; otherwise, how would
  // you allocate it?
  assert((int64_t)Size > 0);
  if (Size != 1)
    Size /= 8;
  return Size;
}

MemoryInstrumenter::MemoryInstrumenter(): ModulePass(ID) {
  MemAllocHook = NULL;
  MainArgsAllocHook = NULL;
  TopLevelHook = NULL;
  AddrTakenHook = NULL;
  GlobalsAllocHook = NULL;
  MemHooksIniter = NULL;
  Main = NULL;
  CharType = LongType = IntType = NULL;
  CharStarType = NULL;
  VoidType = NULL;
}

bool MemoryInstrumenter::isMalloc(Function *F) const {
  vector<string>::const_iterator Pos = find(MallocNames.begin(),
                                            MallocNames.end(),
                                            F->getName());
  return Pos != MallocNames.end();
}

void MemoryInstrumenter::instrumentMainArgs(Module &M) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(Main);
  assert(Main->arg_size() == 0 || Main->arg_size() == 2);

  if (Main->arg_size() == 0)
    return;

  Value *Arg1 = Main->arg_begin();
  assert(Arg1->getType() == IntType);
  Value *Arg2 = ++Main->arg_begin();
  assert(Arg2->getType()->isPointerTy());
  assert(cast<PointerType>(Arg2->getType())->getElementType() == CharStarType);

  Value *Args[3] = {Arg1, Arg2,
    ConstantInt::get(IntType, IDA.getValueID(Arg2))};
  AddedByUs.insert(CallInst::Create(MainArgsAllocHook, Args, "",
                                    Main->begin()->getFirstNonPHI()));
}

void MemoryInstrumenter::instrumentAlloca(AllocaInst *AI) {
  TargetData &TD = getAnalysis<TargetData>();

  // Calculate the type size.
  uint64_t TypeSize = TD.getTypeSizeInBits(AI->getAllocatedType());
  TypeSize = BitLengthToByteLength(TypeSize);

  // Calculate where to insert.
  assert(!AI->isTerminator());
  BasicBlock::iterator Loc = AI; ++Loc;

  // start = alloca type
  // =>
  // start = alloca type
  // HookMemAlloc
  // HookTopLevel
  instrumentMemoryAllocation(AI, ConstantInt::get(LongType, TypeSize), NULL,
                             Loc);
}

void MemoryInstrumenter::instrumentMemoryAllocation(Value *Start,
                                                    Value *Size,
                                                    Value *Success,
                                                    Instruction *Loc) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(Start->getType()->isPointerTy());
  assert(Size);
  // The size argument to HookMemAlloc must be long.
  assert(Size->getType() == LongType);
  assert(Success == NULL || Success->getType()->isIntegerTy(1));
  assert(Loc);

  vector<Value *> Args;
  // Arg 1: value ID
  Args.push_back(ConstantInt::get(IntType, IDA.getValueID(Start)));
  // Arg 2: starting address
  if (Start->getType() != CharStarType) {
    Start = new BitCastInst(Start, CharStarType, "", Loc);
    AddedByUs.insert(cast<Instruction>(Start));
  }
  Args.push_back(Start);
  // Arg 3: bound
  Args.push_back(Size);

  if (Success == NULL) {
    AddedByUs.insert(CallInst::Create(MemAllocHook, Args, "", Loc));
  } else {
    BasicBlock *BB = Loc->getParent();
    BasicBlock *RestBB = BB->splitBasicBlock(Loc, "rest");
    BasicBlock *CallMallocHookBB = BasicBlock::Create(BB->getContext(),
                                                      "call_malloc_hook",
                                                      BB->getParent(),
                                                      RestBB);
    BB->getTerminator()->eraseFromParent();
    AddedByUs.insert(BranchInst::Create(CallMallocHookBB, RestBB, Success, BB));
    AddedByUs.insert(CallInst::Create(MemAllocHook, Args, "",
                                      CallMallocHookBB));
    AddedByUs.insert(BranchInst::Create(RestBB, CallMallocHookBB));
  }
}

void MemoryInstrumenter::instrumentMalloc(const CallSite &CS) {
  TargetData &TD = getAnalysis<TargetData>();

  Function *Callee = CS.getCalledFunction();
  assert(isMalloc(Callee));

  Instruction *Ins = CS.getInstruction();

  // Calculate where to insert.
  // <Loc> will be the next instruction executed.
  BasicBlock::iterator Loc;
  if (!Ins->isTerminator()) {
    Loc = Ins;
    ++Loc;
  } else {
    assert(isa<InvokeInst>(Ins));
    Loc = cast<InvokeInst>(Ins)->getNormalDest()->getFirstNonPHI();
  }

  IRBuilder<> Builder(Loc);
  Value *Start = NULL;
  Value *Size = NULL;
  Value *Success = NULL; // Indicate whether the allocation succeeded.

  StringRef CalleeName = Callee->getName();
  if (CalleeName == "malloc" || CalleeName == "valloc") {
    Start = Ins;
    Size = CS.getArgument(0);
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
    AddedByUs.insert(cast<Instruction>(Success));
  } else if (CalleeName.startswith("_Zn")) {
    Start = Ins;
    Size = CS.getArgument(0);
  } else if (CalleeName == "calloc") {
    // calloc() takes two size_t, i.e. i64.
    // Therefore, no need to worry Mul will have two operands with different
    // types. Also, Size will always be of type i64.
    Start = Ins;
    assert(CS.getArgument(0)->getType() == LongType);
    assert(CS.getArgument(1)->getType() == LongType);
    Size = BinaryOperator::Create(Instruction::Mul,
                                  CS.getArgument(0),
                                  CS.getArgument(1),
                                  "",
                                  Loc);
    AddedByUs.insert(cast<Instruction>(Size));
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
    AddedByUs.insert(cast<Instruction>(Success));
  } else if (CalleeName == "memalign" || CalleeName == "realloc") {
    Start = Ins;
    Size = CS.getArgument(1);
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
    AddedByUs.insert(cast<Instruction>(Success));
  } else if (CalleeName == "strdup") {
    Start = Ins;
    // Use strlen to compute the length of the allocated memory.
    Value *StrLen = EmitStrLen(Ins, Builder, &TD);
    AddedByUs.insert(cast<Instruction>(StrLen));
    // size = strlen(result) + 1
    Size = Builder.CreateAdd(StrLen, ConstantInt::get(LongType, 1));
    AddedByUs.insert(cast<Instruction>(Size));
    Success = Builder.CreateICmpNE(Ins, ConstantPointerNull::get(CharStarType));
    AddedByUs.insert(cast<Instruction>(Success));
  } else if (CalleeName == "getline") {
    // getline(char **lineptr, size_t *n, FILE *stream)
    // start = *lineptr
    // size = *n
    // succ = (<rv> != -1)
    Start = Builder.CreateLoad(CS.getArgument(0));
    AddedByUs.insert(cast<Instruction>(Start));
    Size = Builder.CreateLoad(CS.getArgument(1));
    AddedByUs.insert(cast<Instruction>(Size));
    Success = Builder.CreateICmpNE(Ins, ConstantInt::get(Ins->getType(), -1));
    AddedByUs.insert(cast<Instruction>(Success));
  } else {
    assert(false && "Unhandled malloc function call");
  }

  //      start = malloc(size)
  //      if (success)
  //        HookMemAlloc
  // Loc:
  instrumentMemoryAllocation(Start, Size, Success, Loc);
}

void MemoryInstrumenter::checkFeatures(Module &M) {
  // Check whether any memory allocation function can
  // potentially be pointed by function pointers.
  // Also, all intrinsic functions will be called directly, i.e. not via
  // function pointers.
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (isMalloc(F) || F->isIntrinsic()) {
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
    // 0 is the return, 1 is the first parameter.
    if (F->isDeclaration() && F->doesNotAlias(0) && !isMalloc(F)) {
      errs().changeColor(raw_ostream::RED);
      errs() << F->getName() << "'s return value is marked noalias, ";
      errs() << "but the function is not treated as malloc.\n";
      errs().resetColor();
    }
  }

  // Sequential types except pointer types shouldn't be used as the type of
  // an instruction, a function parameter, or a global variable.
  for (Module::global_iterator GI = M.global_begin(), E = M.global_end();
       GI != E; ++GI) {
    if (isa<SequentialType>(GI->getType()))
      assert(GI->getType()->isPointerTy());
  }
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
      if (isa<SequentialType>(AI->getType()))
        assert(AI->getType()->isPointerTy());
    }
  }
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
        if (isa<SequentialType>(Ins->getType()))
          assert(Ins->getType()->isPointerTy());
      }
    }
  }
}

void MemoryInstrumenter::setupHooks(Module &M) {
  // No existing functions have the same name.
  assert(M.getFunction(MemAllocHookName) == NULL);
  assert(M.getFunction(MainArgsAllocHookName) == NULL);
  assert(M.getFunction(TopLevelHookName) == NULL);
  assert(M.getFunction(AddrTakenHookName) == NULL);
  assert(M.getFunction(GlobalsAllocHookName) == NULL);
  assert(M.getFunction(MemHooksIniterName) == NULL);

  // Setup MemAllocHook.
  vector<Type *> ArgTypes;
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(LongType);
  FunctionType *MemAllocHookType = FunctionType::get(VoidType,
                                                     ArgTypes,
                                                     false);
  MemAllocHook = Function::Create(MemAllocHookType,
                                  GlobalValue::ExternalLinkage,
                                  MemAllocHookName,
                                  &M);

  // Setup MainArgsAllocHook.
  ArgTypes.clear();
  ArgTypes.push_back(IntType);
  ArgTypes.push_back(PointerType::getUnqual(CharStarType));
  ArgTypes.push_back(IntType);
  FunctionType *MainArgsAllocHookType = FunctionType::get(VoidType,
                                                          ArgTypes,
                                                          false);
  MainArgsAllocHook = Function::Create(MainArgsAllocHookType,
                                       GlobalValue::ExternalLinkage,
                                       MainArgsAllocHookName,
                                       &M);

  // Setup MemHooksIniter.
  FunctionType *MemHooksIniterType = FunctionType::get(VoidType, false);
  MemHooksIniter = Function::Create(MemHooksIniterType,
                                    GlobalValue::ExternalLinkage,
                                    MemHooksIniterName,
                                    &M);

  // Setup TopLevelHook.
  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  FunctionType *TopLevelHookType = FunctionType::get(VoidType,
                                                     ArgTypes,
                                                     false);
  TopLevelHook = Function::Create(TopLevelHookType,
                                  GlobalValue::ExternalLinkage,
                                  TopLevelHookName,
                                  &M);

  // Setup AddrTakenHook.
  ArgTypes.clear();
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(CharStarType);
  ArgTypes.push_back(IntType);
  FunctionType *AddrTakenHookType = FunctionType::get(VoidType,
                                                      ArgTypes,
                                                      false);
  AddrTakenHook = Function::Create(AddrTakenHookType,
                                   GlobalValue::ExternalLinkage,
                                   AddrTakenHookName,
                                   &M);

  // Setup GlobalsAccessHook.
  FunctionType *GlobalsAllocHookType = FunctionType::get(VoidType, false);
  GlobalsAllocHook = Function::Create(GlobalsAllocHookType,
                                      GlobalValue::ExternalLinkage,
                                      GlobalsAllocHookName,
                                      &M);
}

void MemoryInstrumenter::setupScalarTypes(Module &M) {
  VoidType = Type::getVoidTy(M.getContext());
  CharType = Type::getInt8Ty(M.getContext());
  CharStarType = PointerType::getUnqual(CharType);
  LongType = Type::getIntNTy(M.getContext(), __WORDSIZE);
  IntType = Type::getInt32Ty(M.getContext());
}

void MemoryInstrumenter::instrumentGlobals(Module &M) {
  TargetData &TD = getAnalysis<TargetData>();

  // Function HookGlobalsAlloc contains only one basic block.
  // The BB iterates through all global variables, and calls HookMemAlloc
  // for each of them.
  BasicBlock *BB = BasicBlock::Create(M.getContext(), "entry",
                                      GlobalsAllocHook);
  Instruction *Ret = ReturnInst::Create(M.getContext(), BB);
  AddedByUs.insert(Ret);

  for (Module::global_iterator GI = M.global_begin(), E = M.global_end();
       GI != E; ++GI) {
    // We are going to delete llvm.global_ctors.
    // Therefore, don't instrument it.
    if (GI->getName() == "llvm.global_ctors")
      continue;
    uint64_t TypeSize = TD.getTypeSizeInBits(GI->getType()->getElementType());
    TypeSize = BitLengthToByteLength(TypeSize);
    instrumentMemoryAllocation(GI, ConstantInt::get(LongType, TypeSize), NULL,
                               Ret);
    instrumentPointer(GI, Ret);
  }

  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    // These hooks added by us don't have a value ID.
    if (MemAllocHook == F || MainArgsAllocHook == F || TopLevelHook == F ||
        AddrTakenHook == F || GlobalsAllocHook == F || MemHooksIniter == F) {
      continue;
    }
    // Ignore intrinsic functions because we cannot take the address of
    // an intrinsic. Also, no function pointers will point to instrinsic
    // functions.
    if (F->isIntrinsic())
      continue;
    uint64_t TypeSize = TD.getTypeSizeInBits(F->getType());
    TypeSize = BitLengthToByteLength(TypeSize);
    assert(TypeSize == TD.getPointerSize());
    instrumentMemoryAllocation(F, ConstantInt::get(LongType, TypeSize), NULL,
                               Ret);
    instrumentPointer(F, Ret);
  }
}

bool MemoryInstrumenter::runOnModule(Module &M) {
  // Initialize the list of memory allocatores.
  MallocNames.push_back("malloc");
  MallocNames.push_back("calloc");
  MallocNames.push_back("valloc");
  MallocNames.push_back("realloc");
  MallocNames.push_back("memalign");
  MallocNames.push_back("_Znwm");
  MallocNames.push_back("_Znaj");
  MallocNames.push_back("_Znam");
  MallocNames.push_back("strdup");
  MallocNames.push_back("getline");

  // Check whether there are unsupported language features.
  checkFeatures(M);

  // Setup scalar types.
  setupScalarTypes(M);

  // Find the main function.
  Main = M.getFunction("main");
  assert(Main && !Main->isDeclaration() && !Main->hasLocalLinkage());

  // Setup hook function declarations.
  setupHooks(M);

  // Hook global variable allocations.
  instrumentGlobals(M);

  // Hook memory allocations and memory accesses.
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (F->isDeclaration())
      continue;
    // The second argument of main(int argc, char *argv[]) needs special
    // handling, which is done in instrumentMainArgs.
    // We should treat argv as a memory allocation instead of a regular
    // pointer.
    if (Main != F)
      instrumentPointerParameters(F);
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I)
        instrumentInstructionIfNecessary(I);
    }
  }

  // main(argc, argv)
  // argv is allocated by outside.
  instrumentMainArgs(M);

  // Lower global constructors.
  lowerGlobalCtors(M);

#if 0
  // Add HookGlobalsAlloc to the global_ctors list.
  addNewGlobalCtor(M);
#endif

  // Call the memory hook initializer and the global variable allocation hook
  // at the very beginning.
  Instruction *OldEntry = Main->begin()->getFirstNonPHI();
  AddedByUs.insert(CallInst::Create(MemHooksIniter, "", OldEntry));
  AddedByUs.insert(CallInst::Create(GlobalsAllocHook, "", OldEntry));

  return true;
}

void MemoryInstrumenter::lowerGlobalCtors(Module &M) {
  // Find llvm.global_ctors.
  GlobalVariable *GV = M.getNamedGlobal("llvm.global_ctors");
  if (!GV)
    return;
  assert(!GV->isDeclaration() && !GV->hasLocalLinkage());

  // Should be an array of '{ int, void ()* }' structs.  The first value is
  // the init priority, which must be 65535 if the bitcode is generated using
  // clang.
  ConstantArray *InitList = dyn_cast<ConstantArray>(GV->getInitializer());
  assert(InitList);
  for (unsigned i = 0, e = InitList->getNumOperands(); i != e; ++i) {
    ConstantStruct *CS =
      dyn_cast<ConstantStruct>(InitList->getOperand(i));
    assert(CS);
    assert(CS->getNumOperands() == 2);

    // Get the priority.
    ConstantInt *Priority = dyn_cast<ConstantInt>(CS->getOperand(0));
    assert(Priority);
    // TODO: For now, we assume all priorities must be 65535.
    assert(Priority->equalsInt(65535));

    // Get the constructor function.
    Constant *FP = CS->getOperand(1);
    if (FP->isNullValue())
      break;  // Found a null terminator, exit.

    // Explicitly call the constructor at the main entry.
    AddedByUs.insert(CallInst::Create(FP, "", Main->begin()->getFirstNonPHI()));
  }

  // Clear the global_ctors array.
  // Use eraseFromParent() instead of removeFromParent().
  GV->eraseFromParent();
}

void MemoryInstrumenter::addNewGlobalCtor(Module &M) {
  // TODO: Could have reused the old StructType, but llvm.global_ctors is not
  // guaranteed to exist.
  // Setup the types.
  vector<Type *> FieldTypes;
  FieldTypes.push_back(IntType);
  FieldTypes.push_back(GlobalsAllocHook->getType());
  StructType *GlobalCtorType = StructType::get(M.getContext(), FieldTypes);
  ArrayType *GlobalCtorsType = ArrayType::get(GlobalCtorType, 1);

  // Setup the intializer.
  vector<Constant *> Fields;
  Fields.push_back(ConstantInt::get(IntType, 65535));
  Fields.push_back(GlobalsAllocHook);
  Constant *GlobalCtor = ConstantStruct::get(GlobalCtorType, Fields);
  Constant *Initializer = ConstantArray::get(GlobalCtorsType, GlobalCtor);

  // Finally, create the global variable.
  new GlobalVariable(M, GlobalCtorsType, true, GlobalValue::AppendingLinkage,
                     Initializer, "llvm.global_ctors");
}

void MemoryInstrumenter::instrumentStoreInst(StoreInst *SI) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  Value *ValueStored = SI->getValueOperand();
  const Type *ValueType = ValueStored->getType();
  if (ValueType == LongType || ValueType->isPointerTy()) {
    vector<Value *> Args;

    if (ValueType == LongType) {
      Instruction *ValueCast = new IntToPtrInst(ValueStored, CharStarType,
                                                "", SI);
      AddedByUs.insert(ValueCast);
      Args.push_back(ValueCast);
    } else {
      Instruction *ValueCast = new BitCastInst(ValueStored, CharStarType,
                                               "", SI);
      AddedByUs.insert(ValueCast);
      Args.push_back(ValueCast);
    }

    Instruction *PointerCast = new BitCastInst(SI->getPointerOperand(),
                                               CharStarType,
                                               "", SI);
    AddedByUs.insert(PointerCast);
    Args.push_back(PointerCast);

    unsigned InsID = IDA.getInstructionID(SI);
    assert(InsID != IDAssigner::INVALID_ID);
    Args.push_back(ConstantInt::get(IntType, InsID));

    AddedByUs.insert(CallInst::Create(AddrTakenHook, Args, "", SI));
  }
}

void MemoryInstrumenter::instrumentInstructionIfNecessary(Instruction *I) {
  DEBUG(dbgs() << "Processing" << *I << "\n";);

  // Skip those instructions added by us.
  if (AddedByUs.count(I))
    return;

  // Instrument pointer stores, i.e. store X *, X **.
  // store long, long * is considered as a pointer store as well.
  if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
    instrumentStoreInst(SI);
    return;
  }

  // Any instructions of a pointer type, including mallocs and AllocaInsts.
  // Call instrumentPointerInstruction before instrumentMalloc so that
  // HookMemAlloc will be added before HookTopLevel which prevents us from
  // using an outdated version number.
  if (I->getType()->isPointerTy())
    instrumentPointerInstruction(I);

  // Instrument memory allocation function calls.
  CallSite CS(I);
  if (CS) {
    // TODO: A function pointer can possibly point to memory allocation
    // or memory free functions. We don't handle this case for now.
    // We added a feature check. The pass will assertion fail upon such cases.
    Function *Callee = CS.getCalledFunction();
    if (Callee && isMalloc(Callee))
      instrumentMalloc(CS);
  }

  // Instrument AllocaInsts.
  if (AllocaInst *AI = dyn_cast<AllocaInst>(I))
    instrumentAlloca(AI);
}

void MemoryInstrumenter::instrumentPointerInstruction(Instruction *I) {
  BasicBlock::iterator Loc;
  if (isa<PHINode>(I)) {
    // Cannot insert hooks right after a PHI, because PHINodes have to be
    // grouped together.
    Loc = I->getParent()->getFirstNonPHI();
  } else if (!I->isTerminator()) {
    Loc = I;
    ++Loc;
  } else {
    assert(isa<InvokeInst>(I));
    Loc = cast<InvokeInst>(I)->getNormalDest()->getFirstNonPHI();
  }
  instrumentPointer(I, Loc);
}

void MemoryInstrumenter::instrumentPointer(Value *V, Instruction *Loc) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(V->getType()->isPointerTy());

  unsigned ValueID = IDA.getValueID(V);
  if (ValueID == IDAssigner::INVALID_ID)
    errs() << *V << "\n";
  assert(ValueID != IDAssigner::INVALID_ID);

  vector<Value *> Args;
  Instruction *Cast = new BitCastInst(V, CharStarType, "", Loc);
  AddedByUs.insert(Cast);
  Args.push_back(Cast);
  Args.push_back(ConstantInt::get(IntType, ValueID));
  AddedByUs.insert(CallInst::Create(TopLevelHook, Args, "", Loc));
}

void MemoryInstrumenter::instrumentPointerParameters(Function *F) {
  assert(F && !F->isDeclaration());
  Instruction *Entry = F->begin()->getFirstNonPHI();
  for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
    if (AI->getType()->isPointerTy())
      instrumentPointer(AI, Entry);
  }
}
