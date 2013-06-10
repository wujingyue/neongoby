#include <string>

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRReader.h"
// necessary to support "-load"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetData.h"

#include "rcs/IDAssigner.h"

#include "dyn-aa/Passes.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

int main(int argc, char *argv[]) {
  sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc, argv);

  cl::ParseCommandLineOptions(argc, argv, "fake opt");

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

  PassManager Passes;
  // MemoryInstrumenter does not initialize required passes. Therefore, we need
  // manually add them. Otherwise, PassManager won't be able to find the
  // required passes.
  const std::string &ModuleDataLayout = M->getDataLayout();
  if (!ModuleDataLayout.empty())
    Passes.add(new TargetData(ModuleDataLayout));
  Passes.add(new IDAssigner());
  Passes.add(createMemoryInstrumenterPass());
  Passes.add(createBitcodeWriterPass(Out.os()));
  Passes.run(*M);

  delete M;

  return 0;
}
