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
            description = 'Check the call graph generated based on the ' \
                    'specified AA')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log')
    aa_choices = ['tbaa', 'basicaa', 'no-aa', 'ds-aa', 'anders-aa', 'bc2bdd-aa']
    parser.add_argument('aa',
            help = 'the underlying alias analysis: ' + str(aa_choices),
            metavar = 'aa',
            choices = aa_choices)
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # Some AAs require additional plugins.
    # TODO: Should be specified in a configuration file.
    if args.aa == 'ds-aa':
        cmd = rcs_utils.load_plugin(cmd, 'LLVMDataStructure')
    elif args.aa == 'anders-aa':
        cmd = rcs_utils.load_plugin(cmd, 'Andersens')
    elif args.aa == 'bc2bdd-aa':
        if not os.path.exists('bc2bdd.conf'):
            sys.stderr.write('\033[1;31m')
            print >> sys.stderr, 'Error: bc2bdd-aa requires bc2bdd.conf, ' \
                    'which cannot be found in the current directory.'
            sys.stderr.write('\033[m')
            sys.exit(1)
        cmd = rcs_utils.load_plugin(cmd, 'bc2bdd')

    cmd = string.join((cmd, '-' + args.aa))
    cmd = string.join((cmd, '-fpcg'))
    cmd = string.join((cmd, '-check-cg'))
    cmd = string.join((cmd, '-log-file', args.log))
    cmd = string.join((cmd, '-disable-output', '<', args.bc))
    
    rcs_utils.invoke(cmd)
