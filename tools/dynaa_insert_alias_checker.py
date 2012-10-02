#!/usr/bin/env python

# Author: Jingyue

import argparse
import string
import dynaa_utils
import rcs_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Insert alias checker')
    parser.add_argument('bc', help = 'the bitcode of the program')
    aa_choices = ['tbaa', 'basicaa', 'no-aa', 'ds-aa', 'anders-aa', 'bc2bdd-aa']
    parser.add_argument('aa',
            help = 'the checked alias analysis: ' + str(aa_choices),
            metavar = 'aa',
            choices = aa_choices)
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # Some AAs require additional plugins.
    # TODO: Should be specified in a configuration file.
    if args.aa == 'ds-aa':
        cmd = rcs_utils.load_plugin(cmd, 'LLVMDataStructure')
        cmd = string.join((cmd, '-intra'))
    elif args.aa == 'basicaa':
        cmd = string.join((cmd, '-intra'))
    elif args.aa == 'anders-aa':
        cmd = rcs_utils.load_plugin(cmd, 'RCSAndersens')
    elif args.aa == 'bc2bdd-aa':
        if not os.path.exists('bc2bdd.conf'):
            sys.stderr.write('\033[1;31m')
            print >> sys.stderr, 'Error: bc2bdd-aa requires bc2bdd.conf, ' \
                    'which cannot be found in the current directory.'
            sys.stderr.write('\033[m')
            sys.exit(1)
        cmd = rcs_utils.load_plugin(cmd, 'bc2bdd')

    cmd = string.join((cmd, '-' + args.aa))
    cmd = string.join((cmd, '-instrument-alias-checker'))
    assert args.bc.endswith('.bc')
    output_bc = args.bc[0:-3] + '.alias_checker.bc'
    cmd = string.join((cmd, '-o', output_bc, '<', args.bc))
    rcs_utils.invoke(cmd)

    cmd = string.join(('clang++',
                       output_bc,
                       '-o',
                       args.bc[0:-3] + '.alias_checker',
                       '-pthread'))
    if args.bc.startswith('pbzip2'):
        cmd = string.join((cmd, '-lbz2'))
    if args.bc.startswith('ferret'):
        cmd = string.join((cmd, '-lgsl', '-lblas'))
    if args.bc.startswith('gpasswd'):
        cmd = string.join((cmd, '-lcrypt'))
    if args.bc.startswith('cvs'):
        cmd = string.join((cmd, '-lcrypt'))
    if args.bc.startswith('mysqld'):
        cmd = string.join((cmd, '-lcrypt', '-ldl', '-lz'))
    rcs_utils.invoke(cmd)
