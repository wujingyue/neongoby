// vim: sw=2

#define DEBUG_TYPE "dyn-aa"

#include <string>
#include <set>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/IntrinsicInst.h"

#include "rcs/typedefs.h"
#include "rcs/IDAssigner.h"
#include "dyn-aa/LogCounter.h"

#include "dyn-aa/Utils.h"

using namespace llvm;
using namespace std;
using namespace rcs;

namespace dyn_aa {
struct TestcaseReducer: public ModulePass, public LogProcessor {
  static char ID;

  TestcaseReducer();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);

  void processBasicBlock(const BasicBlockRecord &Record);

 private:
  set<Function *> FunctionList;
  set<BasicBlock *> BasicBlockList;

  void reduceFunctions();
  void reduceBasicBlocks(Module &M);
};
}

using namespace dyn_aa;

char TestcaseReducer::ID = 0;

static RegisterPass<TestcaseReducer> X("reduce-testcase",
                                       "Reduce testcase for alias pointers",
                                       false, false);

static cl::list<unsigned> ValueIDs("pointer-value",
                                   cl::desc("Value IDs of the two pointers"));

void TestcaseReducer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<IDAssigner>();
}

TestcaseReducer::TestcaseReducer(): ModulePass(ID) {}

void TestcaseReducer::reduceFunctions() {
  for (set<Function *>::iterator F = FunctionList.begin();
       F != FunctionList.end(); ++F) {
    (*F)->deleteBody();
  }
}

void TestcaseReducer::reduceBasicBlocks(Module &M) {
  
}

bool TestcaseReducer::runOnModule(Module &M) {
  // get executed functions and basic blocks from pointer log
  processLog();

  // try to reduce the number of functions in the module to something small.
  reduceFunctions();
  
  // Attempt to delete entire basic blocks at a time to speed up
  // convergence... this actually works by setting the terminator of the blocks
  // to a return instruction then running simplifycfg, which can potentially
  // shrinks the code dramatically quickly
  //
  reduceBasicBlocks(M);

  
  /*
  for (set<BasicBlock *>::iterator BB = BasicBlockList.begin();
       BB != BasicBlockList.end(); ++BB) {
    errs() << "BasicBlock: " << *BB << "\n";
  }
  for (set<Function *>::iterator F = FunctionList.begin();
       F != FunctionList.end(); ++F) {
    errs() << "Function: " << *F << "\n";
  }
   */

  /*
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (F->isDeclaration())
      continue;
    for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
      for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        
      }
    }
  }
   */

  return true;
}

void TestcaseReducer::processBasicBlock(const BasicBlockRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  BasicBlock *BB = dyn_cast<BasicBlock>(IDA.getValue(Record.ValueID));
  
  unsigned OldBBSize = BasicBlockList.size();
  BasicBlockList.insert(BB);
  if (OldBBSize != BasicBlockList.size()) {
    Function *F = BB->getParent();
    FunctionList.insert(F);
  }
}
