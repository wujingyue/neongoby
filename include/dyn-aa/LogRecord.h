// Author: Jingyue

#ifndef __DYN_AA_LOG_RECORD_H
#define __DYN_AA_LOG_RECORD_H

namespace dyn_aa {
enum LogRecordType {
  AddrTakenDecl = 0,
  TopLevelPointTo,
  AddrTakenPointTo
} __attribute__((packed));

struct AddrTakenDeclLogRecord {
  AddrTakenDeclLogRecord() {}
  AddrTakenDeclLogRecord(void *Addr, unsigned long Len, unsigned Alloc):
      Address(Addr), Bound(Len), AllocatedBy(Alloc) {}

  void *Address;
  unsigned long Bound;
  unsigned AllocatedBy;
} __attribute__((packed));

struct TopLevelPointToLogRecord {
  TopLevelPointToLogRecord() {}
  TopLevelPointToLogRecord(unsigned PtrVID, void *PttAddr):
      PointerValueID(PtrVID), PointeeAddress(PttAddr) {}

  unsigned PointerValueID;
  void *PointeeAddress;
} __attribute__((packed));

struct AddrTakenPointToLogRecord {
  AddrTakenPointToLogRecord() {}
  AddrTakenPointToLogRecord(void *PtrAddr, void *PttAddr, unsigned InsID):
      PointerAddress(PtrAddr), PointeeAddress(PttAddr), InstructionID(InsID) {}

  void *PointerAddress;
  void *PointeeAddress;
  // <InstructionID> is not a must, but makes debugging a lot easier. 
  unsigned InstructionID;
} __attribute__((packed));
}

#endif
