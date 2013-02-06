// vim: sw=2

#define DEBUG_TYPE "dyn-aa"

#include <string>
#include <set>

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/LLVMContext.h"

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
  Module *Program;

  void reduceFunctions(Module &M);
  void reduceBasicBlocks(Module &M);
  bool runPasses(Module *Program,
                 const std::vector<std::string> &Passes,
                 std::string &OutputFilename) const;
  void setProgram(Module *M);
  Module *getProgram() const { return Program; }
  bool emitBitCode(const Module *M, const std::string &Filename) const;
};
}

using namespace dyn_aa;

char TestcaseReducer::ID = 0;

static RegisterPass<TestcaseReducer> X("reduce-testcase",
                                       "Reduce testcase for alias pointers",
                                       false, false);

static cl::list<unsigned> ValueIDs("pointer-value",
                                   cl::desc("Value IDs of the two pointers"));

static cl::opt<string> ProgramName("prog-name",
                                   cl::desc("Program name"));

void TestcaseReducer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<IDAssigner>();
}

TestcaseReducer::TestcaseReducer(): ModulePass(ID) {
  Program = NULL;
}

void TestcaseReducer::setProgram(Module *M) {
  if (Program)
    delete Program;
  Program = M;
}

/// emitBitCode - This function is used to output the current Program
/// to a file named "prag-name.reduce.bc".
///
bool TestcaseReducer::emitBitCode(const Module *M,
                                  const std::string &Filename) const {
  std::string ErrInfo;
  tool_output_file Out(Filename.c_str(), ErrInfo, raw_fd_ostream::F_Binary);
  if (ErrInfo.empty()) {
    WriteBitcodeToFile(M, Out.os());
    Out.os().close();
    if (!Out.os().has_error()) {
      Out.keep();
      return true;
    }
  }
  return false;
}

/// runPasses - Run the specified passes on Program, outputting a bitcode file
/// and writing the filename into OutputFile if successful.  If the
/// optimizations fail for some reason (optimizer crashes), return true,
/// otherwise return false.  If DeleteOutput is set to true, the bitcode is
/// deleted on success, and the filename string is undefined.  This prints to
/// outs() a single line message indicating whether compilation was successful
/// or failed.
///
bool TestcaseReducer::runPasses(Module *Program,
                          const std::vector<std::string> &Passes,
                          std::string &OutputFilename) const {
  // setup the output file name
  outs().flush();
  sys::Path uniqueFilename(ProgramName + ".reduce.bc");
  std::string ErrMsg;
  if (uniqueFilename.makeUnique(true, &ErrMsg)) {
    errs() << "Error making unique filename: "
    << ErrMsg << "\n";
    return(1);
  }
  OutputFilename = uniqueFilename.str();

  // set up the input file name
  sys::Path inputFilename(ProgramName + "-input.bc");
  if (inputFilename.makeUnique(true, &ErrMsg)) {
    errs() << "Error making unique filename: "
    << ErrMsg << "\n";
    return(1);
  }

  std::string ErrInfo;
  tool_output_file InFile(inputFilename.c_str(), ErrInfo,
                          raw_fd_ostream::F_Binary);

  if (!ErrInfo.empty()) {
    errs() << "Error opening bitcode file: " << inputFilename.str() << "\n";
    return 1;
  }
  WriteBitcodeToFile(Program, InFile.os());
  InFile.os().close();
  if (InFile.os().has_error()) {
    errs() << "Error writing bitcode file: " << inputFilename.str() << "\n";
    InFile.os().clear_error();
    return 1;
  }

  sys::Path tool = sys::Program::FindProgramByName("opt");
  if (tool.empty()) {
    errs() << "Cannot find `opt' in PATH!\n";
    return 1;
  }

  // Ok, everything that could go wrong before running opt is done.
  InFile.keep();

  // setup the child process' arguments
  SmallVector<const char*, 8> Args;
  std::string Opt = tool.str();
  Args.push_back(Opt.c_str());

  Args.push_back("-o");
  Args.push_back(OutputFilename.c_str());
  std::vector<std::string> pass_args;
  for (std::vector<std::string>::const_iterator I = Passes.begin(),
       E = Passes.end(); I != E; ++I )
    pass_args.push_back( std::string("-") + (*I) );
  for (std::vector<std::string>::const_iterator I = pass_args.begin(),
       E = pass_args.end(); I != E; ++I )
    Args.push_back(I->c_str());
  Args.push_back(inputFilename.c_str());
  Args.push_back(0);

  DEBUG(errs() << "\nAbout to run:\t";
        for (unsigned i = 0, e = Args.size()-1; i != e; ++i)
        errs() << " " << Args[i];
        errs() << "\n";
        );

  sys::Path prog;
  prog = tool;

  // Redirect stdout and stderr to nowhere
  sys::Path Nowhere;
  const sys::Path *Redirects[3] = {0, &Nowhere, &Nowhere};
  int Timeout = 300;
  int MemoryLimit = 100;
  int result = sys::Program::ExecuteAndWait(prog, Args.data(), 0, Redirects,
                                            Timeout, MemoryLimit, &ErrMsg);

  // Remove the temporary input file
  inputFilename.eraseFromDisk();

  // Was the child successful?
  return result != 0;
}

void TestcaseReducer::reduceFunctions(Module &M) {
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration() && !FunctionList.count(F)) {
      F->replaceAllUsesWith(UndefValue::get(F->getType()));
      F->deleteBody();
    }
  }
  errs() << "# of total functions " << M.size() << "\n";
  errs() << "# of deleted functions " << M.size() - FunctionList.size() << "\n";
}

void TestcaseReducer::reduceBasicBlocks(Module &M) {
  unsigned NumBasicBlocks = 0;
  for (Module::iterator F = M.begin(); F != M.end(); ++F) {
    if (!F->isDeclaration()) {
      for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
        ++NumBasicBlocks;
        if (!BasicBlockList.count(BB)
            && BB->getTerminator()->getNumSuccessors()) {
          // Loop over all of the successors of this block, deleting any PHI nodes
          // that might include it.
          for (succ_iterator SI = succ_begin(BB), E = succ_end(BB);
               SI != E; ++SI)
            (*SI)->removePredecessor(BB);

          TerminatorInst *BBTerm = BB->getTerminator();

          if (!BB->getTerminator()->getType()->isVoidTy())
            BBTerm->replaceAllUsesWith(
                Constant::getNullValue(BBTerm->getType()));

          // Replace the old terminator instruction.
          BB->getInstList().pop_back();
          new UnreachableInst(BB->getContext(), BB);
        }
      }
    }
  }
  errs() << "# of total basic blocks " << NumBasicBlocks << "\n";
  errs() << "# of deleted basic blocks "
  << NumBasicBlocks - BasicBlockList.size() << "\n";

  // Now run the CFG simplify pass on the function...
  std::vector<std::string> Passes;
  Passes.push_back("simplifycfg");
  Passes.push_back("verify");

  std::string BitcodeResult;
  runPasses(&M, Passes, BitcodeResult);
  SMDiagnostic Err;
  Module *Result = ParseIRFile(BitcodeResult, Err, getGlobalContext());
  sys::Path(BitcodeResult).eraseFromDisk();  // No longer need the file on disk

  setProgram(Result);
}

bool TestcaseReducer::runOnModule(Module &M) {
  // get executed functions and basic blocks from pointer logs
  processLog();

  // try to reduce the number of functions in the module to something small.
  reduceFunctions(M);

  // Attempt to delete entire basic blocks at a time to speed up
  // convergence... this actually works by setting the terminator of the blocks
  // to a return instruction then running simplifycfg, which can potentially
  // shrinks the code dramatically quickly
  //
  reduceBasicBlocks(M);

  // write bitcode to file
  emitBitCode(getProgram(), ProgramName + ".reduce.bc");
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
