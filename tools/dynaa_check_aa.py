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
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    cmd = dynaa_utils.load_aa(cmd, args.aa)
    # Some AAs don't support inter-procedural alias queries.
    # Add -intra option for them.
    if args.aa == 'ds-aa' or args.aa == 'basicaa':
        cmd = ' '.join((cmd, '-intra'))
    cmd = ' '.join((cmd, '-check-aa'))
    cmd = ' '.join((cmd, '-log-file', args.log))
    # cmd = ' '.join((cmd, '-output-dyn-aliases', '/tmp/dyn-aliases'))
    if args.check_all:
        cmd = ' '.join((cmd, '-check-all-pointers'))
    cmd = ' '.join((cmd, '-stats'))
    cmd = ' '.join((cmd, '-disable-output', '<', args.bc))

    rcs_utils.invoke(cmd)
