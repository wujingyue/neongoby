#!/usr/bin/env python

import argparse
import os
import sys
import string
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description = 'Check the soundness of the specified AA')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log (.pts)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                                str(dynaa_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = dynaa_utils.get_aa_choices())
    parser.add_argument('--check-all',
                        help = 'check all pointers',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--disable-print-value',
                        help = 'disable printing values. only print value IDs',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--root-only',
                        help = 'only print root missing aliases',
                        action = 'store_true',
                        default = False)
    # Due to the behavior of LLVM's alias analysis chaining, the baseline AA
    # must be an ImmutablePass.
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be ' + \
                                'correct: ' + str(dynaa_utils.get_aa_choices()),
                        metavar = 'baseline_aa',
                        default = 'no-aa',
                        choices = ['no-aa', 'basicaa', 'tbaa'])
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # Load the baseline AA
    if args.baseline == args.aa:
        sys.stderr.write('\033[0;31m')
        print >> sys.stderr, 'Error: Baseline and the checked AA',
        print >> sys.stderr, 'must be different'
        sys.stderr.write('\033[m')
        sys.exit(1)
    # baseline need be put before aa
    cmd = dynaa_utils.load_aa(cmd, args.baseline)
    cmd = ' '.join((cmd, '-baseline-aa'))
    cmd = ' '.join((cmd, '-baseline-aa-name', args.baseline))

    # Load the checked AA
    cmd = dynaa_utils.load_aa(cmd, args.aa)

    # Some AAs don't support inter-procedural alias queries.
    # Add -intra or -baseline-intra option for them.
    if dynaa_utils.supports_intra_proc_queries_only(args.aa):
        cmd = ' '.join((cmd, '-intra'))
    if dynaa_utils.supports_intra_proc_queries_only(args.baseline):
        cmd = ' '.join((cmd, '-baseline-intra'))

    cmd = ' '.join((cmd, '-check-aa'))
    cmd = ' '.join((cmd, '-log-file', args.log))
    # cmd = ' '.join((cmd, '-output-dyn-aliases', '/tmp/dyn-aliases'))
    if args.check_all or args.root_only:
        cmd = ' '.join((cmd, '-check-all-pointers'))
    if args.disable_print_value:
        cmd = ' '.join((cmd, '-print-value-in-report=false'))
    if args.root_only:
        cmd = ' '.join((cmd, '-root-only'))
    cmd = ' '.join((cmd, '-stats'))
    cmd = ' '.join((cmd, '-disable-output', '<', args.bc))

    rcs_utils.invoke(cmd)
