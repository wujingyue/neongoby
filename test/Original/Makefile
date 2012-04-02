# Author: Jingyue

PROGS = SimpleTest TestCalloc TestStore TestInit TestThread
BCS = $(PROGS:=.bc)

CFLAGS = -g -Wall
CXXFLAGS = -g -Wall
LDFLAGS = -pthread

all: $(BCS) $(PROGS)

clean:
	rm -f $(BCS)

%.bc: %.c
	clang $< -o $@ -c -emit-llvm $(CFLAGS)

%.bc: %.cpp
	clang++ $< -o $@ -c -emit-llvm $(CXXFLAGS)

%: %.bc
	clang++ $< -o $@ $(LDFLAGS)

.PHONY: clean
