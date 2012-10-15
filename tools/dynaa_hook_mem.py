#!/usr/bin/env python

import argparse
import os
import sys
import string
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Add tracing code for pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('--hook-all',
                        help = 'hook all pointers (False by default)',
                        action = 'store_true',
                        default = False)
    args = parser.parse_args()

    instrumented_bc = args.prog + '.inst.bc'
    instrumented_exe = args.prog + '.inst'

    cmd = dynaa_utils.load_all_plugins('opt')
    cmd = string.join((cmd, '-instrument-memory'))
    if args.hook_all:
        cmd = string.join((cmd, '-hook-all-pointers'))
    cmd = string.join((cmd, '-o', instrumented_bc))
    cmd = string.join((cmd, '<', args.prog + '.bc'))
    rcs_utils.invoke(cmd)

    cmd = string.join(('clang++', instrumented_bc,
                       rcs_utils.get_libdir() + '/libDynAAMemoryHooks.a',
                       '-o', instrumented_exe, '-pthread'))
    extra_linking_flags = dynaa_utils.get_extra_linking_flags(args.bc)
    cmd = string.join((cmd, string.join(extra_linking_flags)))
    rcs_utils.invoke(cmd)
