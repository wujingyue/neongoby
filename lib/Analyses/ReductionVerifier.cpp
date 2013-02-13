#define DEBUG_TYPE "dyn-aa"

#include <cstdio>

#include "llvm/IntrinsicInst.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/IDAssigner.h"

namespace dyn_aa {
struct ReductionVerifier: public ModulePass {
  static char ID;

  ReductionVerifier(): ModulePass(ID) { }
  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
};
}

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dyn_aa;

static RegisterPass<ReductionVerifier> X("verify-reducer",
                                         "Verify whether reducer keeps "
                                         "the bug",
                                         false, // Is CFG Only?
                                         true); // Is Analysis?
// Don't register to AliasAnalysis group. It would confuse CallGraphChecker
// to use ReductionVerifier as the underlying AA to generate the call
// graph.
// static RegisterAnalysisGroup<AliasAnalysis> Y(X);

char ReductionVerifier::ID = 0;

bool ReductionVerifier::runOnModule(Module &M) {
  Function *DeclareFn = Intrinsic::getDeclaration(&M, Intrinsic::dbg_declare);
  for (Value::use_iterator UI = DeclareFn->use_begin();
       UI != DeclareFn->use_end(); ++UI) {
    User *Usr = *UI;
    assert(isa<CallInst>(Usr) || isa<InvokeInst>(Usr));
    CallSite CS(cast<Instruction>(Usr));
    MDNode *MD = cast<MDNode>(CS.getArgument(0));
    Value *V = MD->getOperand(0);
    errs() << *V << "\n";
  }

  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
		for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
			for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
        if (I->getMetadata("alias_id")) {
          errs() << *I << "\n";
        }
			}
		}
	}

  return false;
}

void ReductionVerifier::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
}
