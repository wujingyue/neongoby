#!/usr/bin/env python

# FIXME: general enough to be a script in the rcs project. However, it uses
# dynaa_utils.load_aa for now.

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
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # Load the checked AA
    cmd = dynaa_utils.load_aa(cmd, args.aa)

    cmd = ' '.join((cmd, '-aa-eval'))
    cmd = ' '.join((cmd, '-stats'))
    cmd = ' '.join((cmd, '-disable-output', '<', args.bc))

    rcs_utils.invoke(cmd)
