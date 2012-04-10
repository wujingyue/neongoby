// Author: Jingyue

#define DEBUG_TYPE "instrument-memory"

#include <string>
using namespace std;

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/DenseSet.h"
using namespace llvm;

#include "common/IDAssigner.h"
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
  void instrumentMemoryAllocation(Value *Start, Value *Size, Instruction *Loc);
  void instrumentMalloc(const CallSite &CS);
  void instrumentAlloca(AllocaInst *AI);
  void instrumentStoreInst(StoreInst *SI);
  void instrumentPointer(Value *V, Instruction *Loc);
  void instrumentPointerInstruction(Instruction *I);
  void instrumentPointerParameters(Function *F);
  void instrumentGlobalsAlloc(Module &M);
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
  const IntegerType *CharType, *LongType, *IntType;
  const PointerType *CharStarType;
  const Type *VoidType;
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
  assert(isa<PointerType>(Arg2->getType()));
  assert(cast<PointerType>(Arg2->getType())->getElementType() == CharStarType);
  
  Value *Args[3] = {Arg1, Arg2,
    ConstantInt::get(IntType, IDA.getValueID(Arg2))};
  AddedByUs.insert(CallInst::Create(MainArgsAllocHook, Args, Args + 3,
                                    "", Main->begin()->getFirstNonPHI()));
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
  // HookMemAlloc(ins id, start, sizeof(type))
  instrumentMemoryAllocation(AI, ConstantInt::get(LongType, TypeSize), Loc);
}

void MemoryInstrumenter::instrumentMemoryAllocation(Value *Start, Value *Size,
                                                    Instruction *Loc) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();

  assert(isa<PointerType>(Start->getType()));
  assert(Size->getType() == LongType);
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
  AddedByUs.insert(CallInst::Create(MemAllocHook,
                                    Args.begin(), Args.end(),
                                    "", Loc));
}

void MemoryInstrumenter::instrumentMalloc(const CallSite &CS) {
  Function *Callee = CS.getCalledFunction();
  assert(isMalloc(Callee));
  
  Instruction *Ins = CS.getInstruction();
  // The return type should be (i8 *). 
  assert(Ins->getType() == CharStarType);

  // Calculate where to insert.
  BasicBlock::iterator Loc;
  if (!Ins->isTerminator()) {
    Loc = Ins;
    ++Loc;
  } else {
    assert(isa<InvokeInst>(Ins));
    Loc = cast<InvokeInst>(Ins)->getNormalDest()->getFirstNonPHI();
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
    assert(CS.getArgument(0)->getType() == LongType);
    assert(CS.getArgument(1)->getType() == LongType);
    Size = BinaryOperator::Create(Instruction::Mul,
                                  CS.getArgument(0),
                                  CS.getArgument(1),
                                  "",
                                  Loc);
    AddedByUs.insert(cast<Instruction>(Size));
  } else if (CalleeName == "memalign") {
    Size = CS.getArgument(1);
  } else if (CalleeName == "realloc") {
    // Don't worry about the free feature. We ignore free anyway. 
    Size = CS.getArgument(1);
  }
  assert(Size);
  // The size argument to HookMemAlloc must be long;
  assert(Size->getType() == LongType);

  // start = malloc(size)
  // =>
  // start = malloc(size)
  // HookMemAlloc(start, size)
  instrumentMemoryAllocation(Ins, Size, Loc);
}

void MemoryInstrumenter::checkFeatures(Module &M) {
  // Check whether any memory allocation function can
  // potentially be pointed by function pointers. 
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (isMalloc(F)) {
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
      assert(isMalloc(F));
    }
  }

  // Sequential types except pointer types shouldn't be used as the type of 
  // an instruction, a function parameter, or a global variable. 
  for (Module::global_iterator GI = M.global_begin(), E = M.global_end();
       GI != E; ++GI) {
    if (isa<SequentialType>(GI->getType()))
      assert(isa<PointerType>(GI->getType()));
  }
  for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
    for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
      if (isa<SequentialType>(AI->getType()))
        assert(isa<PointerType>(AI->getType()));
    }
  }
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator Ins = BB->begin(); Ins != BB->end(); ++Ins) {
        if (isa<SequentialType>(Ins->getType()))
          assert(isa<PointerType>(Ins->getType()));
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
  vector<const Type *> ArgTypes;
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

// TODO: Treat each global variable as a pointer as well. 
// Not necessary for now, because they do not create false point-tos. 
void MemoryInstrumenter::instrumentGlobalsAlloc(Module &M) {
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
    // Ignore the intrinsic global variables, such as llvm.used. 
    if (GI->getName().startswith("llvm."))
      continue;
    uint64_t TypeSize = TD.getTypeSizeInBits(GI->getType()->getElementType());
    TypeSize = BitLengthToByteLength(TypeSize);
    instrumentMemoryAllocation(GI, ConstantInt::get(LongType, TypeSize), Ret);
  }
}

bool MemoryInstrumenter::runOnModule(Module &M) {
  // Check whether there are unsupported language features.
  checkFeatures(M);

  // Setup scalar types. 
  setupScalarTypes(M);

  // Find the main function. 
  Main = M.getFunction("main");
  assert(Main && !Main->isDeclaration() && !Main->hasLocalLinkage());

  // Setup hook function declarations. 
  setupHooks(M);

  // Initialize the list of memory allocatores.
  MallocNames.push_back("malloc");
  MallocNames.push_back("calloc");
  MallocNames.push_back("valloc");
  MallocNames.push_back("realloc");
  MallocNames.push_back("memalign");
  MallocNames.push_back("_Znwm");
  MallocNames.push_back("_Znaj");
  MallocNames.push_back("_Znam");

  // Hook global variable allocations. 
  instrumentGlobalsAlloc(M);

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
  vector<const Type *> FieldTypes;
  FieldTypes.push_back(IntType);
  FieldTypes.push_back(GlobalsAllocHook->getType());
  StructType *GlobalCtorType = StructType::get(M.getContext(), FieldTypes);
  ArrayType *GlobalCtorsType = ArrayType::get(GlobalCtorType, 1);

  // Setup the intializer. 
  vector<Constant *> Fields;
  Fields.push_back(ConstantInt::get(IntType, 65535));
  Fields.push_back(GlobalsAllocHook);
  Constant *GlobalCtor = ConstantStruct::get(GlobalCtorType, Fields);
  Constant *Initializer = ConstantArray::get(GlobalCtorsType, &GlobalCtor, 1);

  // Finally, create the global variable. 
  new GlobalVariable(M, GlobalCtorsType, true, GlobalValue::AppendingLinkage,
                     Initializer, "llvm.global_ctors");
}

void MemoryInstrumenter::instrumentStoreInst(StoreInst *SI) {
  Value *ValueStored = SI->getValueOperand();
  const Type *ValueType = ValueStored->getType();
  if (ValueType == LongType || isa<PointerType>(ValueType)) {
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
    AddedByUs.insert(CallInst::Create(AddrTakenHook,
                                      Args.begin(), Args.end(),
                                      "", SI));
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


  // Instrument memory allocation function calls. 
  CallSite CS(I);
  if (CS.getInstruction()) {
    // TODO: A function pointer can possibly point to memory allocation
    // or memroy free functions. We don't handle this case for now. 
    Function *Callee = CS.getCalledFunction();
    if (Callee && isMalloc(Callee)) {
      instrumentMalloc(CS);
      return;
    }
  }

  // Instrument AllocaInsts. 
  if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
    instrumentAlloca(AI);
    return;
  }

  // Regular pointers, i.e. not the results of mallocs or allocs. 
  if (isa<PointerType>(I->getType())) {
    instrumentPointerInstruction(I);
  }
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

  unsigned ValueID = IDA.getValueID(V);
  assert(ValueID != IDAssigner::INVALID_ID);
  
  vector<Value *> Args;
  Instruction *Cast = new BitCastInst(V, CharStarType, "", Loc);
  AddedByUs.insert(Cast);
  Args.push_back(Cast);
  Args.push_back(ConstantInt::get(IntType, ValueID));
  AddedByUs.insert(CallInst::Create(TopLevelHook,
                                    Args.begin(), Args.end(),
                                    "", Loc));
}

void MemoryInstrumenter::instrumentPointerParameters(Function *F) {
  assert(F && !F->isDeclaration());
  Instruction *Entry = F->begin()->getFirstNonPHI();
  for (Function::arg_iterator AI = F->arg_begin(); AI != F->arg_end(); ++AI) {
    if (isa<PointerType>(AI->getType()))
      instrumentPointer(AI, Entry);
  }
}
