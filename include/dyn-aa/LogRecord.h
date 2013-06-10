#ifndef __DYN_AA_LOG_RECORD_H
#define __DYN_AA_LOG_RECORD_H

#include <cstdlib>

// Structs are packed to save space.

namespace neongoby {
struct MemAllocRecord {
  void *Address;
  unsigned long Bound;
  unsigned AllocatedBy;
} __attribute__((packed));

struct TopLevelRecord {
  unsigned PointerValueID;
  void *PointeeAddress;
  // If the pointer is a LoadInst, LoadFrom stores the pointer operand of the
  // LoadInst; otherwise, LoadedFrom is NULL.
  void *LoadedFrom;
} __attribute__((packed));

struct EnterRecord {
  unsigned FunctionID;
} __attribute__((packed));

struct StoreRecord {
  void *PointerAddress;
  void *PointeeAddress;
  // <InstructionID> is not a must, but makes debugging a lot easier. 
  unsigned InstructionID;
} __attribute__((packed));

struct CallRecord {
  unsigned InstructionID;
} __attribute__((packed));

struct ReturnRecord {
  unsigned FunctionID;
  unsigned InstructionID;
} __attribute__((packed));

struct BasicBlockRecord {
  // IDAssigner does not build a BasicBlockMapping,
  // so we use ValueID to identify a basic block
  unsigned ValueID;
} __attribute__((packed));

struct LogRecord {
  enum LogRecordType {
    MemAlloc,
    TopLevel,
    Enter,
    Store,
    Call,
    Return,
    BasicBlock
  } __attribute__((packed));

  LogRecordType RecordType;
  union {
    MemAllocRecord MAR;
    TopLevelRecord TLR;
    EnterRecord ER;
    StoreRecord SR;
    CallRecord CR;
    ReturnRecord RR;
    BasicBlockRecord BBR;
  };
};
} // namespace neongoby

#endif
