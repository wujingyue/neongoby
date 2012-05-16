#!/usr/bin/env python

import os, sys

def get_opt_base():
    return "opt " + \
            "-load $LLVM_ROOT/install/lib/ID.so " + \
            "-load $LLVM_ROOT/install/lib/CFG.so " + \
            "-load $LLVM_ROOT/install/lib/Instrumenters.so "

def invoke(cmd):
    print >> sys.stderr, cmd
    ret = os.system(cmd)
    if ret != 0:
        sys.exit(ret)

if __name__ == "__main__":
    assert len(sys.argv) == 2
    prog_name = sys.argv[1]
    instrumented_bc = prog_name + ".inst.bc"
    instrumented_exe = prog_name + ".inst"

    cmd = get_opt_base()
    cmd += "-instrument-memory "
    cmd += "-o " + instrumented_bc + " "
    cmd += "< " + prog_name + ".bc "
    invoke(cmd)

    cmd = "clang++ " + instrumented_bc + " "
    cmd += "$LLVM_ROOT/install/lib/libMemoryHooks.a "
    cmd += "-o " + instrumented_exe + " -pthread "
    if prog_name.startswith("pbzip2"):
        cmd += "-lbz2 "
    if prog_name.startswith("ferret"):
        cmd += "-lgsl -lblas "
    if prog_name.startswith("gpasswd"):
        cmd += "-lcrypt "
    if prog_name.startswith("cvs"):
        cmd += "-lcrypt "
    invoke(cmd)
