// Print the point-to log in a readable format. 

#include <cstdio>
#include <cassert>

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "rcs/typedefs.h"

#include "dyn-aa/LogDumper.h"

using namespace std;
using namespace llvm;
using namespace rcs;
using namespace neongoby;

int main(int argc, char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv, "Dumps point-to logs");
  LogDumper LD;
  LD.processLog();
  return 0;
}
