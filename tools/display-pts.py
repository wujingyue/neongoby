#!/usr/bin/env python

import argparse, os, sys

def get_base_cmd():
    base_cmd = "opt "
    base_cmd += "-load $LLVM_ROOT/install/lib/id.so "
    base_cmd += "-load $LLVM_ROOT/install/lib/PointerAnalysis.so "
    base_cmd += "-load $LLVM_ROOT/install/lib/DynamicPointerAnalysis.so "
    return base_cmd

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description = "Rebuild the point-to graph from a point-to log")
    parser.add_argument("bc", help = "the bitcode of the program")
    parser.add_argument("log", help = "the point-to log")
    parser.add_argument("-o", metavar = "output",
            help = "the output .dot file", required = True)
    args = parser.parse_args()

    cmd = get_base_cmd()
    cmd += "-dyn-pa "
    cmd += "-draw-point-to "
    cmd += "-log-file " + args.log + " "
    cmd += "-dot " + args.o + " "
    cmd += "-disable-output "
    cmd += "< " + args.bc

    ret = os.system(cmd)
    sys.exit(ret)
