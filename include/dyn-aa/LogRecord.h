// Author: Jingyue

#ifndef __DYN_AA_LOG_RECORD_H
#define __DYN_AA_LOG_RECORD_H

namespace dyn_aa {
enum LogRecordType {
  AddrTakenDecl = 0,
  TopLevelPointTo,
  AddrTakenPointTo
};

struct AddrTakenDeclLogRecord {
  AddrTakenDeclLogRecord() {}
  AddrTakenDeclLogRecord(void *Addr, unsigned long Len, unsigned Alloc):
      Address(Addr), Bound(Len), AllocatedBy(Alloc) {}

  void *Address;
  unsigned long Bound;
  unsigned AllocatedBy;
};

struct TopLevelPointToLogRecord {
  TopLevelPointToLogRecord() {}
  TopLevelPointToLogRecord(unsigned PtrVID, void *PttAddr):
      PointerValueID(PtrVID), PointeeAddress(PttAddr) {}

  unsigned PointerValueID;
  void *PointeeAddress;
};

struct AddrTakenPointToLogRecord {
  AddrTakenPointToLogRecord() {}
  AddrTakenPointToLogRecord(void *PtrAddr, void *PttAddr, unsigned InsID):
      PointerAddress(PtrAddr), PointeeAddress(PttAddr), InstructionID(InsID) {}

  void *PointerAddress;
  void *PointeeAddress;
  unsigned InstructionID;
};
}

#endif
