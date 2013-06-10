#!/usr/bin/env python

import argparse
import rcs_utils
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Add tracing code for pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('--hook-all',
                        help = 'hook all pointers (False by default)',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--diagnose',
                        help = 'instrument for test case reduction and ' + \
                               'trace slicing (False by default)',
                        action = 'store_true',
                        default = False)
    args = parser.parse_args()

    instrumented_bc = args.prog + '.inst.bc'
    instrumented_exe = args.prog + '.inst'

    cmd = ng_utils.load_all_plugins('opt')
    # Preparer doesn't preserve IDAssigner, so we put it after
    # -instrument-memory.
    cmd = ' '.join((cmd, '-instrument-memory', '-prepare'))
    if args.hook_all:
        cmd = ' '.join((cmd, '-hook-all-pointers'))
    if args.diagnose:
        cmd = ' '.join((cmd, '-diagnose'))
    cmd = ' '.join((cmd, '-o', instrumented_bc))
    cmd = ' '.join((cmd, '<', args.prog + '.bc'))
    rcs_utils.invoke(cmd)

    cmd = ' '.join(('clang++', instrumented_bc,
                    rcs_utils.get_libdir() + '/libDynAAMemoryHooks.a',
                    '-o', instrumented_exe))
    linking_flags = rcs_utils.get_linking_flags(args.prog)
    # Memory hooks use pthread functions.
    if '-pthread' not in linking_flags:
        linking_flags.append('-pthread')
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    rcs_utils.invoke(cmd)
