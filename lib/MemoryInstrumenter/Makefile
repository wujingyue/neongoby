LEVEL = ../..

LIBRARYNAME = MemoryInstrumenter

LOADABLE_MODULE = 1
BUILD_ARCHIVE = 1

SOURCES = MemoryInstrumenter.cpp

include $(LEVEL)/Makefile.common

all:: MemoryHooks.bc

MemoryHooks.bc: MemoryHooks.cpp
	clang++ $< -o $@ -c -emit-llvm

clean::
	rm -rf MemoryHooks.bc
