# Author: Jingyue

PROGS = SimpleTest TestCalloc TestStore
BCS = $(PROGS:=.bc)

CFLAGS = -g -Wall
CXXFLAGS = -g -Wall

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
