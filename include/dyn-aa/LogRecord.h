#ifndef __DYN_AA_LOG_RECORD_H
#define __DYN_AA_LOG_RECORD_H

#include <cstdlib>

namespace dyn_aa {
enum LogRecordType {
  AddrTakenDecl = 0,
  TopLevelPointTo,
  AddrTakenPointTo,
  CallInstruction,
  ReturnInstruction
} __attribute__((packed));

struct AddrTakenDeclLogRecord {
  void *Address;
  unsigned long Bound;
  unsigned AllocatedBy;
} __attribute__((packed));

struct TopLevelPointToLogRecord {
  unsigned PointerValueID;
  void *PointeeAddress;
  // If the pointer is a LoadInst, LoadFrom stores the pointer operand of the
  // LoadInst; otherwise, LoadedFrom is NULL.
  void *LoadedFrom;
} __attribute__((packed));

struct AddrTakenPointToLogRecord {
  void *PointerAddress;
  void *PointeeAddress;
  // <InstructionID> is not a must, but makes debugging a lot easier. 
  unsigned InstructionID;
} __attribute__((packed));

struct CallInstructionLogRecord {
  unsigned InstructionID;
} __attribute__((packed));

struct ReturnInstructionLogRecord {
  unsigned InstructionID;
} __attribute__((packed));

struct LogRecord {
  LogRecordType RecordType;
  union {
    AddrTakenDeclLogRecord AddrTakenDecl;
    TopLevelPointToLogRecord TopLevel;
    AddrTakenPointToLogRecord AddrTaken;
    CallInstructionLogRecord Call;
    ReturnInstructionLogRecord Return;
  };
};
} // namespace dyn_aa

#endif
