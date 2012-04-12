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
  AddrTakenDeclLogRecord(void *Addr, unsigned Ver, unsigned Alloc):
      Address(Addr), Version(Ver), AllocatedBy(Alloc) {}

  void *Address;
  unsigned Version;
  unsigned AllocatedBy;
};

struct TopLevelPointToLogRecord {
  TopLevelPointToLogRecord() {}
  TopLevelPointToLogRecord(unsigned PtrVID, unsigned PtrVer,
                           void *PttAddr, unsigned PttVer):
      PointerValueID(PtrVID), PointerVersion(PtrVer),
      PointeeAddress(PttAddr), PointeeVersion(PttVer) {}

  unsigned PointerValueID;
  unsigned PointerVersion;
  void *PointeeAddress;
  unsigned PointeeVersion;
};

struct AddrTakenPointToLogRecord {
  AddrTakenPointToLogRecord() {}
  AddrTakenPointToLogRecord(void *PtrAddr, unsigned PtrVer,
                            void *PttAddr, unsigned PttVer):
      PointerAddress(PtrAddr), PointerVersion(PtrVer),
      PointeeAddress(PttAddr), PointeeVersion(PttVer) {}

  void *PointerAddress;
  unsigned PointerVersion;
  void *PointeeAddress;
  unsigned PointeeVersion;
};
}

#endif
