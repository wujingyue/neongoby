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
using namespace llvm;

namespace dyn_aa {
struct MemoryInstrumenter: public ModulePass {
  static const string MemAllocHookName;
  static const string MemAccessHookName;
  static const string GlobalsAllocHookName;
  static const string MemHooksIniterName;

  static char ID;

  MemoryInstrumenter();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

 private:
  bool isMemoryAllocator(Function *F) const;
  void instrumentInstructionIfNecessary(Instruction *I);
  void instrumentMemoryAllocator(const CallSite &CS);
  void instrumentAlloca(AllocaInst *AI);
  void instrumentGlobalsAlloc(Module &M);
  void checkFeatures(Module &M);
  void setupScalarTypes(Module &M);
  void setupHooks(Module &M);
  void lowerGlobalCtors(Module &M);
  void addNewGlobalCtor(Module &M);

  Function *MemAllocHook, *MemAccessHook, *GlobalsAllocHook, *MemHooksIniter;
  Function *Main;
  const IntegerType *CharType, *LongType, *IntType;
  const PointerType *CharStarType;
  const Type *VoidType;
  vector<string> MemAllocatorNames;
};
}
using namespace dyn_aa;

char MemoryInstrumenter::ID = 0;
const string MemoryInstrumenter::MemAllocHookName = "HookMemAlloc";
const string MemoryInstrumenter::MemAccessHookName = "HookMemAccess";
const string MemoryInstrumenter::GlobalsAllocHookName = "HookGlobalsAlloc";
const string MemoryInstrumenter::MemHooksIniterName = "InitMemHooks";

static RegisterPass<MemoryInstrumenter> X("instrument-memory",
                                          "Instrument memory operations",
                                          false, false);

void MemoryInstrumenter::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetData>();
}

MemoryInstrumenter::MemoryInstrumenter(): ModulePass(ID) {
  MemAllocHook = MemAccessHook = GlobalsAllocHook = MemHooksIniter = NULL;
  Main = NULL;
  CharType = LongType = IntType = NULL;
  CharStarType = NULL;
  VoidType = NULL;
}

bool MemoryInstrumenter::isMemoryAllocator(Function *F) const {
  vector<string>::const_iterator Pos = find(MemAllocatorNames.begin(),
                                            MemAllocatorNames.end(),
                                            F->getName());
  return Pos != MemAllocatorNames.end();
}

void MemoryInstrumenter::instrumentAlloca(AllocaInst *AI) {
  // Calculate the type size. 
  TargetData &TD = getAnalysis<TargetData>();
  uint64_t TypeSize = TD.getTypeSizeInBits(AI->getAllocatedType());
  assert(TypeSize % 8 == 0);
  assert((int64_t)TypeSize > 0);
  TypeSize /= 8;

  // start = alloca type
  // =>
  // start = alloca type
  // HookMemAlloc(start, sizeof(type))
  assert(!AI->isTerminator());
  BasicBlock::iterator Loc = AI; ++Loc;
  vector<Value *> Args;
  Args.push_back(new BitCastInst(AI, CharStarType, "", Loc));
  Args.push_back(ConstantInt::get(LongType, TypeSize));
  CallInst::Create(MemAllocHook, Args.begin(), Args.end(), "", Loc);
}

void MemoryInstrumenter::instrumentMemoryAllocator(const CallSite &CS) {
  Function *Callee = CS.getCalledFunction();
  assert(isMemoryAllocator(Callee));
  
  // Calculate where to insert.
  Instruction *Ins = CS.getInstruction();
  // The return type must be (i8 *);
  // otherwise cannot call HookMemAlloc directly. 
  assert(Ins->getType() == CharStarType);
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
    assert(CS.getArgument(0)->getType() == LongType);
    assert(CS.getArgument(1)->getType() == LongType);
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
  // The size argument to HookMemAlloc must be long;
  assert(Size->getType() == LongType);
  
  // start = malloc(size)
  // =>
  // start = malloc(size)
  // HookMemAlloc(start, size)
  vector<Value *> Args;
  Args.push_back(Ins);
  Args.push_back(Size);
  CallInst::Create(MemAllocHook, Args.begin(), Args.end(), "", Loc);
}

void MemoryInstrumenter::checkFeatures(Module &M) {
  // Check whether any memory allocation function can
  // potentially be pointed by function pointers. 
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (isMemoryAllocator(F)) {
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

void MemoryInstrumenter::setupHooks(Module &M) {
  // No existing functions have the same name. 
  assert(M.getFunction(MemAllocHookName) == NULL);
  assert(M.getFunction(MemAccessHookName) == NULL);
  assert(M.getFunction(GlobalsAllocHookName) == NULL);
  assert(M.getFunction(MemHooksIniterName) == NULL);

  // Setup MemAllocHook. 
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

  // Setup MemHooksIniter. 
  FunctionType *MemHooksIniterType = FunctionType::get(VoidType, false);
  MemHooksIniter = Function::Create(MemHooksIniterType,
                                    GlobalValue::ExternalLinkage,
                                    MemHooksIniterName,
                                    &M);
  
  // Setup MemAccessHook. 
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

void MemoryInstrumenter::instrumentGlobalsAlloc(Module &M) {
  // Function HookGlobalsAlloc contains only one basic block. 
  // The BB iterates through all global variables, and calls HookMemAlloc
  // for each of them. 
  BasicBlock *BB = BasicBlock::Create(M.getContext(), "entry",
                                      GlobalsAllocHook);
  for (Module::global_iterator GI = M.global_begin(), E = M.global_end();
       GI != E; ++GI) {
    // Ignore the intrinsic global variables, such as llvm.used. 
    if (GI->getName().startswith("llvm."))
      continue;
    TargetData &TD = getAnalysis<TargetData>();
    uint64_t TypeSize = TD.getTypeSizeInBits(GI->getType()->getElementType());
    assert(TypeSize % 8 == 0);
    assert((int64_t)TypeSize > 0);
    TypeSize /= 8;

    vector<Value *> Args;
    Args.push_back(new BitCastInst(GI, CharStarType, "", BB));
    Args.push_back(ConstantInt::get(LongType, TypeSize));
    CallInst::Create(MemAllocHook, Args.begin(), Args.end(), "", BB);
  }
  ReturnInst::Create(M.getContext(), BB);
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
  MemAllocatorNames.push_back("malloc");
  MemAllocatorNames.push_back("calloc");
  MemAllocatorNames.push_back("valloc");
  MemAllocatorNames.push_back("realloc");
  MemAllocatorNames.push_back("memalign");
  MemAllocatorNames.push_back("_Znwm");
  MemAllocatorNames.push_back("_Znaj");
  MemAllocatorNames.push_back("_Znam");

  // Hook global variable allocations. 
  instrumentGlobalsAlloc(M);

  // Hook memory allocations and memory accesses. 
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I)
        instrumentInstructionIfNecessary(I);
    }
  }

  // Lower global constructors. 
  lowerGlobalCtors(M);

#if 0
  // Add HookGlobalsAlloc to the global_ctors list. 
  addNewGlobalCtor(M);
#endif

  // Call the memory hook initializer and the global variable allocation hook
  // at the very beginning. 
  Instruction *OldEntry = Main->begin()->getFirstNonPHI();
  CallInst::Create(MemHooksIniter, "", OldEntry);
  CallInst::Create(GlobalsAllocHook, "", OldEntry);

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
    CallInst::Create(FP, "", Main->begin()->getFirstNonPHI());
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

void MemoryInstrumenter::instrumentInstructionIfNecessary(Instruction *I) {
  DEBUG(dbgs() << "Processing" << *I << "\n";);

  // Instrument memory allocation function calls. 
  CallSite CS(I);
  if (CS.getInstruction()) {
    // TODO: A function pointer can possibly point to memory allocation
    // or memroy free functions. We don't handle this case for now. 
    Function *Callee = CS.getCalledFunction();
    if (isMemoryAllocator(Callee)) {
      instrumentMemoryAllocator(CS);
    }
    return;
  }

  // Instrument AllocaInsts. 
  if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
    instrumentAlloca(AI);
    return;
  }

  // Instrument pointer stores, i.e. store X *, X **. 
  // store long, long * is considered as a pointer store as well. 
  if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
    Value *ValueStored = SI->getValueOperand();
    const Type *ValueType = ValueStored->getType();
    if (ValueType == LongType || isa<PointerType>(ValueType)) {
      vector<Value *> Args;
      if (ValueType == LongType) {
        Args.push_back(new IntToPtrInst(ValueStored,
                                        CharStarType,
                                        "",
                                        I));
      } else {
        Args.push_back(new BitCastInst(ValueStored, CharStarType, "", I));
      }
      Args.push_back(new BitCastInst(SI->getPointerOperand(),
                                     CharStarType,
                                     "",
                                     I));
      CallInst::Create(MemAccessHook, Args.begin(), Args.end(), "", I);
    }
    return;
  }
}
