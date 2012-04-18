// Author: Jingyue

#include <cstdio>
#include <set>
using namespace std;

#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#include "common/IDAssigner.h"
#include "common/PointerAnalysis.h"
using namespace rcs;

namespace dyn_aa {
struct PointToDrawer: public ModulePass {
  static char ID;

  PointToDrawer(): ModulePass(ID) {}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  virtual bool runOnModule(Module &M);
  virtual void print(raw_ostream &O, const Module *M) const;
};
}
using namespace dyn_aa;

static cl::opt<string> DotFileName("dot",
                                   cl::desc("The output graph file name"
                                            " (.dot)"),
                                   cl::ValueRequired);

char PointToDrawer::ID = 0;

void PointToDrawer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<IDAssigner>();
  AU.addRequired<PointerAnalysis>();
}

bool PointToDrawer::runOnModule(Module &M) {
  IDAssigner &IDA = getAnalysis<IDAssigner>();
  PointerAnalysis &PA = getAnalysis<PointerAnalysis>();

  assert(DotFileName != "");
  FILE *DotFile = fopen(DotFileName.c_str(), "w");
  fprintf(DotFile, "strict digraph PointTo {\n");

  set<unsigned> PointerVids, PointeeVids;
  unsigned NumValues = IDA.getNumValues();
  for (unsigned PointerVid = 0; PointerVid < NumValues; ++PointerVid) {
    Value *Pointer = IDA.getValue(PointerVid);
    assert(Pointer);
    ValueList Pointees;
    PA.getPointees(Pointer, Pointees);
    for (size_t j = 0; j < Pointees.size(); ++j) {
      Value *Pointee = Pointees[j];
      unsigned PointeeVid = IDA.getValueID(Pointee);
      assert(PointeeVid != IDAssigner::INVALID_ID);
      fprintf(DotFile, "TopLevel%u -> AddrTaken%u\n", PointerVid, PointeeVid);
      PointerVids.insert(PointerVid);
      PointeeVids.insert(PointeeVid);
    }
  }
  
  for (set<unsigned>::iterator I = PointerVids.begin(); I != PointerVids.end();
       ++I) {
    fprintf(DotFile, "TopLevel%u [label = %u]\n", *I, *I);
  }
  for (set<unsigned>::iterator I = PointeeVids.begin(); I != PointeeVids.end();
       ++I) {
    fprintf(DotFile,
            "AddrTaken%u [label = %u, style = filled, fillcolor = yellow]\n", 
            *I, *I);
  }

  fprintf(DotFile, "}\n");
  fclose(DotFile);

  return false;
}

void PointToDrawer::print(raw_ostream &O, const Module *M) const {
  // Do nothing. 
}

static RegisterPass<PointToDrawer> X("draw-point-to",
                                     "Draw point-to graphs",
                                     false, // Is CFG Only? 
                                     true); // Is Analysis? 
