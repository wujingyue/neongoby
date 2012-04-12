// Author: Jingyue

// Extracts the point-to graph from the point-to log, and outputs the graph
// in .dot format. 
// TODO: Only outputs static point-to graphs for now. 

// Caution: This piece of code may not compile with gcc-4.3 or earlier. 

#include <cstdio>
#include <cassert>
#include <vector>
using namespace std;

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
using namespace llvm;

#include "dyn-aa/LogRecord.h"
using namespace dyn_aa;

// Map: (address, version) => allocator's value ID
DenseMap<pair<void *, unsigned>, unsigned> AddrTakenDecls;
// Use DenseSet instead of vector, because they are usually lots of 
// duplicated edges. 
// (pointer vid, pointee vid)
DenseSet<pair<unsigned, unsigned> > TopLevelPointTos;
DenseSet<pair<unsigned, unsigned> > AddrTakenPointTos;

static void ProcessAddrTakenDecl(const AddrTakenDeclLogRecord &Record) {
  pair<void *, unsigned> Key(Record.Address, Record.Version);
  assert(!AddrTakenDecls.count(Key) && "Shouldn't be declared twice");
  AddrTakenDecls[Key] = Record.AllocatedBy;
}

static void ProcessTopLevelPointTo(const TopLevelPointToLogRecord &Record) {
  pair<void *, unsigned> PttKey(Record.PointeeAddress, Record.PointeeVersion);
  assert(AddrTakenDecls.count(PttKey));
  TopLevelPointTos.insert(make_pair(Record.PointerValueID,
                                    AddrTakenDecls.lookup(PttKey)));
}

static void ProcessAddrTakenPointTo(const AddrTakenPointToLogRecord &Record) {
  pair<void *, unsigned> PtrKey(Record.PointerAddress, Record.PointerVersion);
  pair<void *, unsigned> PttKey(Record.PointeeAddress, Record.PointeeVersion);
  assert(AddrTakenDecls.count(PtrKey));
  assert(AddrTakenDecls.count(PttKey));
  AddrTakenPointTos.insert(make_pair(AddrTakenDecls.lookup(PtrKey),
                                     AddrTakenDecls.lookup(PttKey)));
}

static void ReadLog() {
  LogRecordType RecordType;
  while (fread(&RecordType, sizeof RecordType, 1, stdin) == 1) {
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          ProcessAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          ProcessTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
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
}

static void WriteDot() {
  // "strict digraph" groups duplicated edges. 
  printf("strict digraph PointTo {\n");

  // Output nodes. 
  // AddrTakenDecls may contain duplicated vids, because one value may span
  // through multiple bytes. We do not want to output duplicated vids. 
  vector<unsigned> AddrTakenVids;
  for (DenseMap<pair<void *, unsigned>, unsigned>::iterator
       I = AddrTakenDecls.begin(), E = AddrTakenDecls.end(); I != E; ++I) {
    AddrTakenVids.push_back(I->second);
  }
  sort(AddrTakenVids.begin(), AddrTakenVids.end());
  AddrTakenVids.resize(unique(AddrTakenVids.begin(), AddrTakenVids.end()) -
                       AddrTakenVids.begin());
  for (size_t i = 0; i < AddrTakenVids.size(); ++i) {
    printf("AddrTaken%d [label = %d, style = filled, fillcolor = yellow]\n",
           AddrTakenVids[i], AddrTakenVids[i]);
  }
  for (DenseSet<pair<unsigned, unsigned> >::iterator
       I = TopLevelPointTos.begin(); I != TopLevelPointTos.end(); ++I) {
    printf("TopLevel%d [label = %d]\n", I->first, I->first);
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
