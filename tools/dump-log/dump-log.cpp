// Author: Jingyue

// Print the point-to log in a readable format. 

#include <cstdio>
#include <cassert>
using namespace std;

#include "dyn-aa/LogRecord.h"
using namespace dyn_aa;

static void PrintAddrTakenDecl(
    const AddrTakenDeclLogRecord &Record) {
  printf("%p, %u, %u\n", Record.Address, Record.Version, Record.AllocatedBy);
}

static void PrintTopLevelPointTo(const TopLevelPointToLogRecord &Record) {
  printf("%u, %u => %p, %u\n",
         Record.PointerValueID, Record.PointerVersion,
         Record.PointeeAddress, Record.PointeeVersion);
}

static void PrintAddrTakenPointTo(const AddrTakenPointToLogRecord &Record) {
  printf("%p, %u => %p, %u\n",
         Record.PointerAddress, Record.PointerVersion,
         Record.PointeeAddress, Record.PointeeVersion);
}

int main(int argc, char *argv[]) {
  LogRecordType RecordType;
  while (fread(&RecordType, sizeof RecordType, 1, stdin) == 1) {
    switch (RecordType) {
      case AddrTakenDecl:
        {
          AddrTakenDeclLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          PrintAddrTakenDecl(Record);
        }
        break;
      case TopLevelPointTo:
        {
          TopLevelPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          PrintTopLevelPointTo(Record);
        }
        break;
      case AddrTakenPointTo:
        {
          AddrTakenPointToLogRecord Record;
          assert(fread(&Record, sizeof Record, 1, stdin) == 1);
          PrintAddrTakenPointTo(Record);
        }
        break;
      default:
        assert(false && "Unknown record type");
    }
  }
  return 0;
}
