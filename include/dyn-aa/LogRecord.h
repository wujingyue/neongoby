#ifndef __DYN_AA_LOG_RECORD_H
#define __DYN_AA_LOG_RECORD_H

#include <cstdlib>

// Structs are packed to save space.

namespace dyn_aa {
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
  unsigned InstructionID;
} __attribute__((packed));

struct LogRecord {
  enum LogRecordType {
    MemAlloc,
    TopLevel,
    Enter,
    Store,
    Call,
    Return
  } __attribute__((packed));

  LogRecordType RecordType;
  pthread_t ThreadID;
  union {
    MemAllocRecord MAR;
    TopLevelRecord TLR;
    EnterRecord ER;
    StoreRecord SR;
    CallRecord CR;
    ReturnRecord RR;
  };
};
} // namespace dyn_aa

#endif
