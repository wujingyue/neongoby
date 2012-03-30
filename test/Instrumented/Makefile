# Author: Jingyue

PROGS = SimpleTest
BCS = $(PROGS:=.bc)
INSTRUMENTED_BCS = $(PROGS:=.inst.bc)
INSTRUMENTED_LLS = $(PROGS:=.inst.ll)

CFLAGS = -g -Wall
CXXFLAGS = -g -Wall

all: $(INSTRUMENTED_BCS) $(INSTRUMENTED_LLS)

clean:
	rm -rf $(INSTRUMENTED_BCS)

%.inst.bc: ../Original/%.bc
	opt -load $(LLVM_ROOT)/install/lib/MemoryInstrumenter.so \
		-instrument-memory \
		-o $@ \
		< $<

%.ll: %.bc
	llvm-dis $<

%.bc: %.c
	clang %< -o $@ -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	clang++ $< -o $@ -c -emit-llvm $(CXXFLAGS)

%: %.bc
	clang++ $< -o $@ $(LDFLAGS)

.PHONY: clean
