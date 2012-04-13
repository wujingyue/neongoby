# Author: Jingyue

PROGS = SimpleTest TestCalloc TestStore TestInit TestThread TestTopLevel
PROGS += TestFP
BCS = $(PROGS:=.bc)
LLS = $(PROGS:=.ll)

CFLAGS = -g -Wall
CXXFLAGS = -g -Wall
LDFLAGS = -pthread

all: $(BCS) $(LLS) $(PROGS)

clean:
	rm -f $(BCS) $(LLS) $(PROGS)

%.bc: %.c
	clang $< -o $@ -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	clang++ $< -o $@ -c -emit-llvm $(CXXFLAGS)

%.ll: %.bc
	llvm-dis $<

%: %.bc
	clang++ $< -o $@ $(LDFLAGS)

.PHONY: clean
