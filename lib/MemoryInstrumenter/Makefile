LEVEL = ../..

LIBRARYNAME = MemoryInstrumenter

LOADABLE_MODULE = 1
BUILD_ARCHIVE = 1

SOURCES = MemoryInstrumenter.cpp

include $(LEVEL)/Makefile.common

all:: MemoryHooks.bc

install:: all

MemoryHooks.bc: MemoryHooks.cpp
	clang++ $< -o $@ -c -emit-llvm -g -O3 -MMD -I$(LEVEL)/include

clean::
	rm -rf MemoryHooks.bc

-include: MemoryHook.d
