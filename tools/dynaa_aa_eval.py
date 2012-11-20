#!/usr/bin/env python

# Author: Jingyue

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
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                                str(dynaa_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = dynaa_utils.get_aa_choices())
    # Due to the behavior of LLVM's alias analysis chaining, the baseline AA
    # must be an ImmutablePass.
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be ' + \
                                'correct: ' + str(dynaa_utils.get_aa_choices()),
                        metavar = 'baseline_aa',
                        choices = ['no-aa', 'basicaa', 'tbaa'])
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # Load the baseline AA if specified
    if args.baseline is not None:
        if args.baseline == args.aa:
            sys.stderr.write('\033[1;31m')
            print >> sys.stderr, 'Error: Baseline and the checked AA',
            print >> sys.stderr, 'must be different'
            sys.stderr.write('\033[m')
            sys.exit(1)
        # baseline need be put before aa
        cmd = dynaa_utils.load_aa(cmd, args.baseline)
        cmd = ' '.join((cmd, '-baseline-aa-name', args.baseline))

    # Load the checked AA
    cmd = dynaa_utils.load_aa(cmd, args.aa)

    cmd = ' '.join((cmd, '-aa-eval'))
    cmd = ' '.join((cmd, '-stats'))
    cmd = ' '.join((cmd, '-disable-output', '<', args.bc))

    rcs_utils.invoke(cmd)
