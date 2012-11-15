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
		-load `llvm-config --prefix`/lib/RCSID.so \
		-load `llvm-config --prefix`/lib/RCSCFG.so \
		-load `llvm-config --prefix`/lib/DynAAInstrumenters.so \
		-instrument-memory \
		-o $@ \
		< $<

%.ll: %.bc
	llvm-dis $<

%.inst: %.inst.bc
	clang++ $< `llvm-config --prefix`/lib/libDynAAMemoryHooks.a -o $@ $(LDFLAGS)

.PHONY: clean
