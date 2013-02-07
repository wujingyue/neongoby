// vim: sw=2

#define DEBUG_TYPE "dyn-aa"

#include <string>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"
#include "llvm/ADT/DenseSet.h"

#include "rcs/typedefs.h"
#include "rcs/IDAssigner.h"

#include "dyn-aa/LogProcessor.h"
#include "dyn-aa/Utils.h"

using namespace llvm;
using namespace std;
using namespace rcs;

namespace dyn_aa {
struct Reducer: public ModulePass, public LogProcessor {
  static char ID;

  Reducer();
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);
  void processBasicBlock(const BasicBlockRecord &Record);

 private:
  DenseSet<Function *> ExecutedFunctions;
  DenseSet<BasicBlock *> ExecutedBasicBlocks;

  void reduceFunctions(Module &M);
  void reduceBasicBlocks(Module &M);
};
}

using namespace dyn_aa;

char Reducer::ID = 0;

static RegisterPass<Reducer> X("remove-untouched-code",
                               "Remove untouched functions and basic blocks",
                               false, false);

static cl::list<unsigned> ValueIDs("pointer-value",
                                   cl::desc("Value IDs of the two pointers"));

void Reducer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<IDAssigner>();
}

Reducer::Reducer(): ModulePass(ID) { }

void Reducer::reduceFunctions(Module &M) {
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration() && !ExecutedFunctions.count(F)) {
      F->replaceAllUsesWith(UndefValue::get(F->getType()));
      F->deleteBody();
    }
  }
  errs() << "# of total functions " << M.size() << "\n";
  errs() << "# of deleted functions " << M.size() - ExecutedFunctions.size()
      << "\n";
}

void Reducer::reduceBasicBlocks(Module &M) {
  unsigned NumBasicBlocks = 0;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration()) {
      BasicBlock *UnreachableBB = BasicBlock::Create(F->getContext(), "", F);
      new UnreachableInst(UnreachableBB->getContext(), UnreachableBB);
      for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
        ++NumBasicBlocks;
        for (succ_iterator SI = succ_begin(BB), E = succ_end(BB); SI != E;
             ++SI) {
          if (!ExecutedBasicBlocks.count(*SI)) {
            BB->getTerminator()->setSuccessor(SI.getSuccessorIndex(),
                                              UnreachableBB);
          }
        }
      }
    }
  }
  errs() << "# of total basic blocks " << NumBasicBlocks << "\n";
  errs() << "# of deleted basic blocks "
      << NumBasicBlocks - ExecutedBasicBlocks.size() << "\n";
}

bool Reducer::runOnModule(Module &M) {
  // get executed functions and basic blocks from pointer logs
  processLog();

  // try to reduce the number of functions in the module to something small.
  reduceFunctions(M);

  // Attempt to delete entire basic blocks at a time to speed up
  // convergence... this actually works by setting the terminator of the blocks
  // to a return instruction then running simplifycfg, which can potentially
  // shrinks the code dramatically quickly
  reduceBasicBlocks(M);

  return true;
}

void Reducer::processBasicBlock(const BasicBlockRecord &Record) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  BasicBlock *BB = cast<BasicBlock>(IDA.getValue(Record.ValueID));

  unsigned OldBBSize = ExecutedBasicBlocks.size();
  ExecutedBasicBlocks.insert(BB);
  if (OldBBSize != ExecutedBasicBlocks.size()) {
    Function *F = BB->getParent();
    ExecutedFunctions.insert(F);
  }
}
