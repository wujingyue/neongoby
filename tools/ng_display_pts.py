#!/usr/bin/env python

import argparse
import os
import sys
import string
import rcs_utils
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description = 'Rebuild the point-to graph from a point-to log')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log')
    parser.add_argument('-o', metavar = 'output',
            help = 'the output .dot file', required = True)
    parser.add_argument('--dsaa',
            help = 'use DSAA as a reference',
            action = 'store_true')
    args = parser.parse_args()

    cmd = ng_utils.load_all_plugins('opt')
    if args.dsaa:
        # Use DSAA
        cmd = rcs_utils.load_plugin(cmd, 'LLVMDataStructure')
        cmd = string.join((cmd, '-tbaa'))
        cmd = string.join((cmd, '-basicaa'))
        cmd = string.join((cmd, '-ds-aa'))
        cmd = string.join((cmd, '-basic-pa'))
        cmd = string.join((cmd, '-draw-point-to'))
    else:
        # Use DynamicPointerAnalysis
        cmd = string.join((cmd, '-dyn-pa'))
        cmd = string.join((cmd, '-draw-point-to'))
        cmd = string.join((cmd, '-log-file', args.log))
    cmd = string.join((cmd, '-dot', args.o))
    cmd = string.join((cmd, '-pointer-stats'))
    cmd = string.join((cmd, '-disable-output'))
    cmd = string.join((cmd, '<', args.bc))

    rcs_utils.invoke(cmd)
