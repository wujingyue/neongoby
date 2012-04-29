# Author: Jingyue

PROGS = SimpleTest TestCalloc TestStore TestInit TestThread TestTopLevel
PROGS += TestFP
BCS = $(PROGS:=.bc)
INSTRUMENTED_BCS = $(PROGS:=.inst.bc)
INSTRUMENTED_LLS = $(PROGS:=.inst.ll)
INSTRUMENTED_EXES = $(PROGS:=.inst)

CFLAGS = -g -Wall
CXXFLAGS = -g -Wall
LDFLAGS = -pthread

all: $(INSTRUMENTED_BCS) $(INSTRUMENTED_LLS) $(INSTRUMENTED_EXES)

clean:
	rm -rf $(INSTRUMENTED_BCS) $(INSTRUMENTED_LLS) $(INSTRUMENTED_EXES)

%.inst.bc: ../Original/%.bc
	opt \
		-load $(LLVM_ROOT)/install/lib/ID.so \
		-load $(LLVM_ROOT)/install/lib/MemoryInstrumenter.so \
		-instrument-memory \
		-o $@ \
		< $<

%.ll: %.bc
	llvm-dis $<

%.inst: %.inst.bc
	clang++ $< ../../lib/MemoryInstrumenter/MemoryHooks.bc -o $@ $(LDFLAGS)

.PHONY: clean
