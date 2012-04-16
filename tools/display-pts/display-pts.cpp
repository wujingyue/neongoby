// Author: Jingyue

// Extracts the point-to graph from the point-to log, and outputs the graph
// in .dot format. 
// TODO: Only outputs static point-to graphs for now. 

// Caution: This piece of code may not compile with gcc-4.3 or earlier. 

#include <iostream>
#include <cstdio>
#include <cassert>
#include <vector>
#include <map>
#include <set>
using namespace std;

#include "llvm/ADT/DenseSet.h"
using namespace llvm;

#include "common/IDAssigner.h"
using namespace rcs;

#include "dyn-aa/LogRecord.h"
using namespace dyn_aa;

typedef pair<unsigned long, unsigned long> Interval;

struct IntervalComparer {
  bool operator()(const Interval &I1, const Interval &I2) const {
    return I1.second <= I2.first;
  }
};

typedef map<Interval, unsigned, IntervalComparer> IntervalTree;

IntervalTree AddrTakenDecls;
// The value IDs of all allocators ever occured. 
// Note that this set is not equivalent to the value set of <AddrTakenDecls>,
// because <AddrTakenDecls> changes from time to time. 
set<unsigned> AddrTakenVids;
// Use DenseSet instead of vector, because they are usually lots of 
// duplicated edges. 
// (pointer vid, pointee vid)
DenseSet<pair<unsigned, unsigned> > TopLevelPointTos;
DenseSet<pair<unsigned, unsigned> > AddrTakenPointTos;

static void ProcessAddrTakenDecl(const AddrTakenDeclLogRecord &Record) {
  unsigned long Start = (unsigned long)Record.Address;
  Interval I(Start, Start + Record.Bound);
  pair<IntervalTree::iterator, IntervalTree::iterator> ER =
      AddrTakenDecls.equal_range(I);
  AddrTakenDecls.erase(ER.first, ER.second);
  AddrTakenDecls.insert(make_pair(I, Record.AllocatedBy));
  AddrTakenVids.insert(Record.AllocatedBy);
}

// Returns the value ID of <Addr>'s allocator. 
// Possible allocators include malloc function calls, AllocaInsts, and
// global variables. 
static unsigned LookupAddress(void *Addr) {
  Interval I((unsigned long)Addr, (unsigned long)Addr + 1);
  IntervalTree::iterator Pos = AddrTakenDecls.find(I);
  if (Pos == AddrTakenDecls.end())
    return IDAssigner::INVALID_ID;
  return Pos->second;
}

static void ProcessTopLevelPointTo(const TopLevelPointToLogRecord &Record) {
  unsigned PointeeVID = LookupAddress(Record.PointeeAddress);
  if (PointeeVID != IDAssigner::INVALID_ID)
    TopLevelPointTos.insert(make_pair(Record.PointerValueID, PointeeVID));
}

static void ProcessAddrTakenPointTo(const AddrTakenPointToLogRecord &Record) {
  unsigned PointerVID = LookupAddress(Record.PointerAddress);
  unsigned PointeeVID = LookupAddress(Record.PointeeAddress);
  assert(PointerVID != IDAssigner::INVALID_ID);
  if (PointeeVID != IDAssigner::INVALID_ID)
    AddrTakenPointTos.insert(make_pair(PointerVID, PointeeVID));
}

static void ReadLog() {
  LogRecordType RecordType;
  int numRecords = 0;
  int numAddrTakenDecls = 0;
  int numAddrTakenPointTos = 0;
  int numTopLevelPointTos = 0;
  while (fread(&RecordType, sizeof RecordType, 1, stdin) == 1) {
    if (numRecords % 1000000 == 0)
      cerr << "Processed " << numRecords << " records\n";
    ++numRecords;
    switch (RecordType) {
      case AddrTakenDecl:
        {
          ++numAddrTakenDecls;
          AddrTakenDeclLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          ProcessAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          ++numTopLevelPointTos;
          TopLevelPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          ProcessTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
          ++numAddrTakenPointTos;
          AddrTakenPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          ProcessAddrTakenPointTo(Record);
        }
        break;
      default:
        fprintf(stderr, "RecordType = %d\n", RecordType);
        assert(false && "Unknown record type");
    }
  }
  cerr << "Processed " << numRecords << " records\n";
  cerr << "# of addr-taken decls = " << numAddrTakenDecls << "\n";
  cerr << "# of addr-taken point-tos = " << numAddrTakenPointTos << "\n";
  cerr << "# of top-level point-tos = " << numTopLevelPointTos << "\n";
}

static void WriteDot() {
  // "strict digraph" groups duplicated edges. 
  printf("strict digraph PointTo {\n");

  // Output nodes. 
  for (set<unsigned>::iterator I = AddrTakenVids.begin();
       I != AddrTakenVids.end(); ++I) {
    printf("AddrTaken%u [label = %u, style = filled, fillcolor = yellow]\n",
           *I, *I);
  }
  for (DenseSet<pair<unsigned, unsigned> >::iterator
       I = TopLevelPointTos.begin(); I != TopLevelPointTos.end(); ++I) {
    printf("TopLevel%u [label = %u]\n", I->first, I->first);
  }

  // Output edges. 
  for (DenseSet<pair<unsigned, unsigned> >::iterator
       I = TopLevelPointTos.begin(); I != TopLevelPointTos.end(); ++I) {
    printf("TopLevel%d -> AddrTaken%d\n", I->first, I->second);
  }
  for (DenseSet<pair<unsigned, unsigned> >::iterator
       I = AddrTakenPointTos.begin(); I != AddrTakenPointTos.end(); ++I) {
    printf("AddrTaken%d -> AddrTaken%d\n", I->first, I->second);
  }
  printf("}\n");
}

int main(int argc, char *argv[]) {
  ReadLog();
  WriteDot();
  return 0;
}
