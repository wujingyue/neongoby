#!/usr/bin/env python

import argparse
import ng_utils
import rcs_utils
import sys
import time

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Insert alias checker')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('aa',
            help = 'the checked alias analysis: ' + \
                    str(ng_utils.get_aa_choices()),
            metavar = 'aa',
            choices = ng_utils.get_aa_choices())
    # Due to the behavior of LLVM's alias analysis chaining, the baseline AA
    # must be an ImmutablePass.
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
                        # help = 'input file containing alias checks')
                        help = argparse.SUPPRESS)
    parser.add_argument('--output-alias-checks',
                        # help = 'output file containing alias checks')
                        help = argparse.SUPPRESS)
    args = parser.parse_args()

    # Initialize output file names after each stage.
    bc_orig = args.prog + '.bc'
    bc_ac = args.prog + '.ac.bc'
    bc_ac_opt = args.prog + '.ac.opt.bc'
    bc_ac_opt_inline = args.prog + '.ac.opt.inline.bc'

    # Insert alias checks.
    time_start_inserting = time.time()
    cmd = ng_utils.load_all_plugins('opt')

    if args.input_alias_checks is not None:
        # If alias checks are inputed by users, we don't need to run any AA
        cmd = ' '.join((cmd, '-input-alias-checks', args.input_alias_checks))
    else:
        if args.baseline == args.aa:
            sys.stderr.write('\033[0;31m')
            print >> sys.stderr, 'Error: Baseline and the checked AA',
            print >> sys.stderr, 'must be different'
            sys.stderr.write('\033[m')
            sys.exit(1)
        cmd = ng_utils.load_aa(cmd, args.baseline)
        cmd = ' '.join((cmd, '-baseline-aa'))
        cmd = ' '.join((cmd, '-baseline-aa-name', args.baseline))
        cmd = ng_utils.load_aa(cmd, args.aa)
    if args.output_alias_checks is not None:
        cmd = ' '.join((cmd, '-output-alias-checks', args.output_alias_checks))
    if args.no_phi:
        cmd = ' '.join((cmd, '-no-phi'))
    cmd = ' '.join((cmd, '-action=' + args.action_if_missed))
    if args.check_all:
        cmd = ' '.join((cmd, '-check-all-pointers-online'))

    cmd = ' '.join((cmd, '-instrument-alias-checker', '-prepare'))
    # Output stats by default.
    cmd = ' '.join((cmd, '-stats'))
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
        cmd = ng_utils.load_all_plugins('opt')
        cmd = ' '.join((cmd, '-inline-alias-checker'))
        cmd = ' '.join((cmd, '-o', bc_ac_opt_inline, '<', bc_ac_opt))
    rcs_utils.invoke(cmd)

    # Codegen.
    time_start_codegen = time.time()
    cmd = ' '.join(('clang++', bc_ac_opt_inline))
    # ReportMissingAlias won't be inlined anyway, so we have to link
    # libDynAAAliasChecker.a
    cmd = ' '.join((cmd, rcs_utils.get_libdir() + '/libDynAAAliasChecker.a'))
    cmd = ' '.join((cmd, '-o', args.prog + '.ac'))
    linking_flags = rcs_utils.get_linking_flags(args.prog)
    # The pthread library is necessary for the online mode.
    linking_flags.append('-pthread')
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    rcs_utils.invoke(cmd)

    # Print runtime of each stage.
    time_finish = time.time()
    print 'Inserting alias checks:', time_start_o3 - time_start_inserting
    print 'Running O3:', time_start_inlining - time_start_o3
    print 'Inlining:', time_start_codegen - time_start_inlining
    print 'Codegen:', time_finish - time_start_codegen
