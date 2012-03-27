/**
 * Author: Jingyue
 */

#define DEBUG_TYPE "instrument-memory"

#include <string>
using namespace std;

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/DerivedTypes.h"
using namespace llvm;

namespace dyn_aa {
struct MemoryInstrumenter: public FunctionPass {
  static const string MemAllocHookName, MemFreeHookName;

  static char ID;

  MemoryInstrumenter(): FunctionPass(ID) {
    MemAllocHook = MemFreeHook = NULL;
    CharType = LongType = NULL;
    VoidType = NULL;
  }
  virtual bool doInitialization(Module &M);
  virtual bool runOnFunction(Function &F);

 private:
  bool isMemoryAllocator(Function *F) const;
  bool isMemoryFreer(Function *F) const;

  Function *MemAllocHook, *MemFreeHook;
  const IntegerType *CharType, *LongType;
  const Type *VoidType;
  vector<string> MemAllocatorNames, MemFreerNames;
};
}
using namespace dyn_aa;

char MemoryInstrumenter::ID = 0;
const string MemAllocHookName = "hook_mem_alloc";
const string MemFreeHookName = "hook_mem_free";

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

bool MemoryInstrumenter::doInitialization(Module &M) {
  // No existing functions have the same name. 
  assert(M.getFunction(MemAllocHookName) == NULL);
  assert(M.getFunction(MemFreeHookName) == NULL);

  // Setup scalar types.
  VoidType = Type::getVoidTy(M.getContext());
  CharType = IntegerType::get(M.getContext(), 8);
  LongType = IntegerType::get(M.getContext(), __WORDSIZE);
  // Setup function types. 
  vector<const Type *> ArgTypes(1, LongType);
  FunctionType *MemAllocHookType = FunctionType::get(CharType, ArgTypes, false);
  MemAllocHook = Function::Create(MemAllocHookType,
                                  GlobalValue::ExternalLinkage,
                                  MemAllocHookName,
                                  &M);
  FunctionType *MemFreeHookType = FunctionType::get(VoidType, false);
  MemFreeHook = Function::Create(MemFreeHookType,
                                 GlobalValue::ExternalLinkage,
                                 MemFreeHookName,
                                 &M);

  // Initialize the list of memory allocatores.
  MemAllocatorNames.push_back("malloc");
  MemAllocatorNames.push_back("calloc");
  MemAllocatorNames.push_back("valloc");
  // TODO: more
  // Initialize the list of memory freers. 
  MemFreerNames.push_back("free");
  // TODO: more

  return true;
}

bool MemoryInstrumenter::runOnFunction(Function &F) {
  return true;
}
