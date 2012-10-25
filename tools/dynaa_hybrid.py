#!/usr/bin/env python

# Author: Jingyue

import argparse
import dynaa_utils
import rcs_utils
import sys
import time
import os
import string

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Insert alias checker')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')

    # online alias checker args
    parser.add_argument('aa',
            help = 'the checked alias analysis: ' + \
                    str(dynaa_utils.get_aa_choices()),
            metavar = 'aa',
            choices = dynaa_utils.get_aa_choices())
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be correct: ' + \
                                str(dynaa_utils.get_aa_choices()),
                        metavar = 'baseline_aa',
                        choices = dynaa_utils.get_aa_choices())
    parser.add_argument('--disable-inline',
                        help = 'do not inline the alias checks',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--disable-opt',
                        help = 'do not run standard compiler optimization',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--no-phi',
                        help = 'store pointers into slots and load them later',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--abort-if-missed',
                        help = 'abort the program on detecting a missed alias',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--input-alias-checks',
                        help = 'input file containing alias checks')
    parser.add_argument('--output-alias-checks',
                        help = 'output file containing alias checks')

    # offline alias checker args
    parser.add_argument('--hook-all',
                        help = 'hook all pointers (False by default)',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--hook-fork',
                        help = 'hook fork() and vfork() (False by default)',
                        action = 'store_true',
                        default = False)

    # hybrid checker args
    parser.add_argument('--offline-funcs',
                        help = 'functions to be checked offline',
                        action = 'append',
                        nargs = '+')

    args = parser.parse_args()

    # Initialize output file names after each stage.
    bc_orig = args.prog + '.bc'
    bc_ac = args.prog + '.ac.bc'
    bc_ac_opt = args.prog + '.ac.opt.bc'
    bc_ac_opt_inline = args.prog + '.ac.opt.inline.bc'
    hybrid_bc = args.prog + '.hybrid.bc'
    hybrid_exe = args.prog + '.hybrid'

    time_start_inserting = time.time()
    # Insert alias checks.
    cmd = dynaa_utils.load_all_plugins('opt')
    if args.input_alias_checks is None:
        # If alias checks are inputed by users, we don't need to run any AA
        if args.baseline is None:
            cmd = dynaa_utils.load_aa(cmd, args.aa)
        else:
            if args.baseline == args.aa:
                sys.stderr.write('\033[1;31m')
                print >> sys.stderr, 'Error: Baseline and the checked AA',
                print >> sys.stderr, 'must be different'
                sys.stderr.write('\033[m')
                sys.exit(1)
            # baseline need be put before aa
            cmd = dynaa_utils.load_aa(cmd, args.baseline, args.aa)
    cmd = ' '.join((cmd, '-instrument-alias-checker'))
    if args.no_phi:
        cmd = ' '.join((cmd, '-no-phi'))
    if args.abort_if_missed:
        cmd = ' '.join((cmd, '-abort-if-missed'))
    if args.input_alias_checks is not None:
        cmd = ' '.join((cmd, '-input-alias-checks', args.input_alias_checks))
    if args.output_alias_checks is not None:
        cmd = ' '.join((cmd, '-output-alias-checks', args.output_alias_checks))
    # ignore offline functions
    if args.offline_funcs is not None:
        for funcs in args.offline_funcs:
            for func in funcs:
                cmd = ' '.join((cmd, '-online-black-list', func))
    cmd = ' '.join((cmd, '-o', bc_ac, '<', bc_orig))
    rcs_utils.invoke(cmd)

    # Run standard optimizations.
    # Don't mix -O3 with the previous command line, because the previous command
    # line may not be using basicaa which can potentially make -O3 pretty slow.
    time_start_o3 = time.time()
    if args.disable_opt:
        cmd = ' '.join(('cp', bc_ac, bc_ac_opt))
    else:
        cmd = ' '.join(('opt', '-O3', '-o', bc_ac_opt, '<', bc_ac))
    rcs_utils.invoke(cmd)

    # Inline alias checks.
    time_start_inlining = time.time()
    if args.disable_inline:
        cmd = ' '.join(('cp', bc_ac_opt, bc_ac_opt_inline))
    else:
        cmd = dynaa_utils.load_all_plugins('opt')
        cmd = ' '.join((cmd, '-inline-alias-checker'))
        cmd = ' '.join((cmd, '-o', bc_ac_opt_inline, '<', bc_ac_opt))
    rcs_utils.invoke(cmd)

    # offline stage
    time_start_hook = time.time()
    cmd = dynaa_utils.load_all_plugins('opt')
    cmd = string.join((cmd, '-instrument-memory'))
    if args.hook_all:
        cmd = string.join((cmd, '-hook-all-pointers'))
    if args.hook_fork:
        cmd = string.join((cmd, '-hook-fork'))

    # only hook offline functions
    if args.offline_funcs is not None:
        for funcs in args.offline_funcs:
            for func in funcs:
                cmd = ' '.join((cmd, '-hook-white-list', func))

    cmd = string.join((cmd, '-o', hybrid_bc))
    cmd = string.join((cmd, '<', bc_ac_opt_inline))
    rcs_utils.invoke(cmd)

    # Codegen.
    time_start_codegen = time.time()
    cmd = ' '.join(('clang++', hybrid_bc))
    # ReportMissingAlias won't be inlined anyway, so we have to link
    # libDynAAAliasChecker.a
    cmd = ' '.join((cmd, rcs_utils.get_libdir() + '/libDynAAAliasChecker.a'))
    cmd = ' '.join((cmd, rcs_utils.get_libdir() + '/libDynAAMemoryHooks.a'))
    cmd = ' '.join((cmd, '-o', hybrid_exe))
    linking_flags = dynaa_utils.get_linking_flags(args.prog)
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    rcs_utils.invoke(cmd)

    # Print runtime of each stage.
    time_finish = time.time()
    print 'Inserting alias checks:', time_start_o3 - time_start_inserting
    print 'Running O3:', time_start_inlining - time_start_o3
    print 'Inlining:', time_start_hook - time_start_inlining
    print 'Hooking:', time_start_codegen - time_start_hook
    print 'Codegen:', time_finish - time_start_codegen
