#!/usr/bin/env python

# Author: Jingyue

import argparse
import string
import dynaa_utils
import rcs_utils
import sys

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Insert alias checker')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('aa',
            help = 'the checked alias analysis: ' + \
                    str(dynaa_utils.get_aa_choices()),
            metavar = 'aa',
            choices = dynaa_utils.get_aa_choices())
    parser.add_argument('--max-alias-checks',
                       help = 'maximum number of alias checks to add',
                       type = int)
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be correct: ' + \
                                str(dynaa_utils.get_aa_choices()),
                        metavar = 'baseline_aa',
                        choices = dynaa_utils.get_aa_choices())
    parser.add_argument('--inline',
                        help = 'whether to inline AssertNoAlias',
                        action = 'store_true',
                        default = False)
    args = parser.parse_args()

    bc_orig = args.prog + '.bc'
    bc_with_alias_checker_no_opt = args.prog + '.alias_checker_no_opt.bc'
    bc_with_alias_checker = args.prog + '.alias_checker.bc'
    bc_inlined = args.prog + '.inlined.bc'

    cmd = dynaa_utils.load_all_plugins('opt')
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
    cmd = string.join((cmd, '-instrument-alias-checker'))
    if args.max_alias_checks is not None:
        cmd = string.join((cmd, '-max-alias-checks', str(args.max_alias_checks)))
    cmd = string.join((cmd,
                       '-o', bc_with_alias_checker_no_opt,
                       '<', bc_orig))
    rcs_utils.invoke(cmd)

    # Don't mix -O3 with the previous command line, because the previous command
    # line may not be using basicaa which can potentially make -O3 pretty slow.
    cmd = string.join(('opt', '-O3',
                       '-o', bc_with_alias_checker,
                       '<', bc_with_alias_checker_no_opt))
    rcs_utils.invoke(cmd)

    if args.inline:
        cmd = dynaa_utils.load_all_plugins('opt')
        cmd = string.join((cmd, '-inline-alias-checker'))
        cmd = string.join((cmd,
                           '-o', bc_inlined,
                           '<', bc_with_alias_checker))
        rcs_utils.invoke(cmd)

    cmd = 'clang++'
    if args.inline:
        cmd = string.join((cmd, bc_inlined))
        cmd = string.join((cmd, '-o', bc_inlined[0:-3]))
    else:
        cmd = string.join((cmd,
                           bc_with_alias_checker,
                           rcs_utils.get_libdir() + '/libDynAAAliasChecker.a'))
        cmd = string.join((cmd, '-o', bc_with_alias_checker[0:-3]))
    linking_flags = dynaa_utils.get_linking_flags(args.prog)
    cmd = string.join((cmd, string.join(linking_flags)))
    rcs_utils.invoke(cmd)
