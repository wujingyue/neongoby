#include <string>

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/PassNameParser.h"
// necessary to support "-load"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/BaselineAliasAnalysis.h"
#include "dyn-aa/Passes.h"
#include "dyn-aa/TraceSlicer.h"
#include "dyn-aa/Reducer.h"
#include "dyn-aa/ReductionVerifier.h"


using namespace std;
using namespace llvm;
using namespace rcs;
using namespace dynaa;

static cl::list<const PassInfo*, bool, PassNameParser>
PassList(cl::desc("Optimizations available:"));

// return 1 if verified, 0 if not verified, -1 if no reduction available
int reduce(Module *M, const vector<bool> &ReductionOptions, Pass *BW = NULL) {
  // initialize passes
  Reducer *R = new Reducer();
  if (!R->setReductionOptions(ReductionOptions)) {
    // all reduction methods are tried
    return -1;
  }
  ReductionVerifier *V = new ReductionVerifier();
  Pass *AA = NULL;
  assert(PassList.size() == 1);
  for (unsigned i = 0; i < PassList.size(); ++i) {
    const PassInfo *PassInf = PassList[i];
    Pass *P = 0;
    if (PassInf->getNormalCtor())
      P = PassInf->getNormalCtor()();
    if (P) {
      AA = P;
    }
  }
  assert(AA);

  PassManager Passes;

  const std::string &ModuleDataLayout = M->getDataLayout();
  if (!ModuleDataLayout.empty())
    Passes.add(new TargetData(ModuleDataLayout));
  Passes.add(R);
  Passes.add(AA);
  Passes.add(V);
  if (BW) {
    Passes.add(BW);
  }
  Passes.run(*M);
  delete M;
  return V->getVerified();
}

int main(int argc, char *argv[]) {
  sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc, argv);

  cl::ParseCommandLineOptions(argc, argv, "fake opt");

  // Initialize passes
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeCore(Registry);
  initializeScalarOpts(Registry);
  initializeVectorization(Registry);
  initializeIPO(Registry);
  initializeAnalysis(Registry);
  initializeIPA(Registry);
  initializeTransformUtils(Registry);
  initializeInstCombine(Registry);
  initializeInstrumentation(Registry);
  initializeTarget(Registry);

  SMDiagnostic Err;
  Module *M = ParseIRFile("-", Err, getGlobalContext());
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  string ErrorInfo;
  tool_output_file Out("-", ErrorInfo, raw_fd_ostream::F_Binary);
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << "\n";
    return 1;
  }

  errs() << "Tagging ...\n";
  PassManager TaggingPasses;
  const std::string &ModuleDataLayout = M->getDataLayout();
  if (!ModuleDataLayout.empty())
    TaggingPasses.add(new TargetData(ModuleDataLayout));
  TaggingPasses.add(new TraceSlicer());
  TaggingPasses.run(*M);

  errs() << "Reducing on tested module ...\n";
  vector<bool> ReductionOptions;
  while (true) {
    Module *TestedModule = llvm::CloneModule(M);
    ReductionOptions.push_back(true);
    int result = reduce(TestedModule, ReductionOptions);
    if (result == -1) {
      // no reduction available
      ReductionOptions.pop_back();
      break;
    } else if (result == 0) {
      // not verified
      errs() << "Disable reduction " << ReductionOptions.size() << "\n";
      ReductionOptions.pop_back();
      ReductionOptions.push_back(false);
    }
    errs() << "\n";
  }

  errs() << "Reducing on real module ...\n";
  Pass * BW = createBitcodeWriterPass(Out.os());
  reduce(M, ReductionOptions, BW);

  return 0;
}
