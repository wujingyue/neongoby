#!/usr/bin/env python

import argparse
import ng_utils
import rcs_utils
import sys
import time
import string

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Insert alias checker')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')

    # online alias checker args
    parser.add_argument('aa',
            help = 'the checked alias analysis: ' + \
                    str(ng_utils.get_aa_choices()),
            metavar = 'aa',
            choices = ng_utils.get_aa_choices())
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be ' + \
                                'correct: ' + str(ng_utils.get_aa_choices()),
                        metavar = 'baseline_aa',
                        default = 'no-aa',
                        choices = ['no-aa', 'basicaa', 'tbaa'])
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
    parser.add_argument('--action-if-missed',
                        help = 'action on detecting a missed alias ' + \
                                '(default: silence)',
                        default = 'silence',
                        choices = ['abort', 'report', 'silence'])
    parser.add_argument('--check-all',
                        help = 'check all pointers',
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
    bc_hybrid = args.prog + '.hybrid.bc'
    bc_hybrid_opt = args.prog + '.hybrid.opt.bc'
    bc_hybrid_opt_inline = args.prog + '.hybrid.opt.inline.bc'
    exe_hybrid = args.prog + '.hybrid'

    time_start_hybrid = time.time()
    # Insert alias checks.
    cmd = ng_utils.load_all_plugins('opt')
    if args.input_alias_checks is not None:
        cmd = ' '.join((cmd, '-input-alias-checks', args.input_alias_checks))
    else:
        if args.baseline == args.aa:
            sys.stderr.write('\033[0;31m')
            print >> sys.stderr, 'Error: Baseline and the checked AA',
            print >> sys.stderr, 'must be different'
            sys.stderr.write('\033[m')
            sys.exit(1)
        # baseline need be put before aa
        cmd = ng_utils.load_aa(cmd, args.baseline)
        cmd = ' '.join((cmd, '-baseline-aa'))
        cmd = ' '.join((cmd, '-baseline-aa-name', args.baseline))
        cmd = ng_utils.load_aa(cmd, args.aa)
    if args.output_alias_checks is not None:
        cmd = ' '.join((cmd, '-output-alias-checks', args.output_alias_checks))
    cmd = ' '.join((cmd, '-instrument-alias-checker'))
    if args.no_phi:
        cmd = ' '.join((cmd, '-no-phi'))
    cmd = ' '.join((cmd, '-action=' + args.action_if_missed))
    if args.check_all:
        cmd = ' '.join((cmd, '-check-all-pointers-online'))
    # ignore offline functions
    if args.offline_funcs is not None:
        for funcs in args.offline_funcs:
            for func in funcs:
                cmd = ' '.join((cmd, '-online-black-list', func))

    cmd = string.join((cmd, '-instrument-memory'))
    if args.hook_all:
        cmd = string.join((cmd, '-hook-all-pointers'))
    if args.hook_fork:
        cmd = string.join((cmd, '-hook-fork'))

    # only hook offline functions
    if args.offline_funcs is not None:
        for funcs in args.offline_funcs:
            for func in funcs:
                cmd = ' '.join((cmd, '-offline-white-list', func))

    # run the preparer
    cmd = ' '.join((cmd, '-prepare'))
    # output stats by default
    cmd = ' '.join((cmd, '-stats'))

    cmd = string.join((cmd, '-o', bc_hybrid))
    cmd = string.join((cmd, '<', bc_orig))
    rcs_utils.invoke(cmd)

    # Run standard optimizations.
    # Don't mix -O3 with the previous command line, because the previous command
    # line may not be using basicaa which can potentially make -O3 pretty slow.
    time_start_o3 = time.time()
    if args.disable_opt:
        cmd = ' '.join(('cp', bc_hybrid, bc_hybrid_opt))
    else:
        cmd = ' '.join(('opt', '-O3', '-o', bc_hybrid_opt, '<', bc_hybrid))
    rcs_utils.invoke(cmd)

    # Inline alias checks.
    time_start_inlining = time.time()
    if args.disable_inline:
        cmd = ' '.join(('cp', bc_hybrid_opt, bc_hybrid_opt_inline))
    else:
        cmd = ng_utils.load_all_plugins('opt')
        cmd = ' '.join((cmd, '-inline-alias-checker'))
        cmd = ' '.join((cmd, '-o', bc_hybrid_opt_inline, '<', bc_hybrid_opt))
    rcs_utils.invoke(cmd)

    # Codegen.
    time_start_codegen = time.time()
    cmd = ' '.join(('clang++', bc_hybrid_opt_inline))
    # ReportMissingAlias won't be inlined anyway, so we have to link
    # libDynAAAliasChecker.a
    cmd = ' '.join((cmd, rcs_utils.get_libdir() + '/libDynAAAliasChecker.a'))
    cmd = ' '.join((cmd, rcs_utils.get_libdir() + '/libDynAAMemoryHooks.a'))
    cmd = ' '.join((cmd, '-o', exe_hybrid))
    linking_flags = rcs_utils.get_linking_flags(args.prog)
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    rcs_utils.invoke(cmd)

    # Print runtime of each stage.
    time_finish = time.time()
    print 'Inserting alias checks and hooking memory access:', \
        time_start_o3 - time_start_hybrid
    print 'Running O3:', time_start_inlining - time_start_o3
    print 'Inlining:', time_start_codegen - time_start_inlining
    print 'Codegen:', time_finish - time_start_codegen
