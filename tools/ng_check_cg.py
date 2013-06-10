#!/usr/bin/env python

import argparse
import os
import sys
import string
import rcs_utils
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description = 'Check the call graph generated based on the ' \
                    'specified AA')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log')
    parser.add_argument('aa',
            help = 'the underlying alias analysis: ' + \
                    str(ng_utils.get_aa_choices()),
            metavar = 'aa',
            choices = ng_utils.get_aa_choices())
    args = parser.parse_args()

    cmd = ng_utils.load_all_plugins('opt')
    cmd = ng_utils.load_aa(cmd, args.aa)
    cmd = string.join((cmd, '-fpcg'))
    cmd = string.join((cmd, '-check-cg'))
    cmd = string.join((cmd, '-log-file', args.log))
    cmd = string.join((cmd, '-disable-output', '<', args.bc))
    
    rcs_utils.invoke(cmd)
