#!/usr/bin/env python

import argparse, os, sys

def get_base_cmd():
    base_cmd = "opt -debug "
    base_cmd += "-load $LLVM_ROOT/install/lib/id.so "
    base_cmd += "-load $LLVM_ROOT/install/lib/PointerAnalysis.so "
    base_cmd += "-load $LLVM_ROOT/install/lib/DynamicPointerAnalysis.so "
    base_cmd += "-load $LLVM_ROOT/install/lib/LLVMDataStructure.so "
    return base_cmd

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description = "Rebuild the point-to graph from a point-to log")
    parser.add_argument("bc", help = "the bitcode of the program")
    parser.add_argument("log", help = "the point-to log")
    parser.add_argument("-o", metavar = "output",
            help = "the output .dot file", required = True)
    parser.add_argument("--dsaa",
            help = "use DSAA as a reference",
            action = "store_true")
    args = parser.parse_args()

    cmd = get_base_cmd()
    if args.dsaa:
        # Use DSAA
        cmd += "-tbaa "
        cmd += "-basicaa "
        cmd += "-ds-aa "
        cmd += "-basic-pa "
        cmd += "-draw-point-to "
    else:
        # Use DynamicPointerAnalysis
        cmd += "-dyn-pa "
        cmd += "-draw-point-to "
        cmd += "-log-file " + args.log + " "
    cmd += "-dot " + args.o + " "
    cmd += "-pointer-stats "
    cmd += "-disable-output "
    cmd += "< " + args.bc

    ret = os.system(cmd)
    sys.exit(ret)
