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
    parser.add_argument('--max-alias-checks',
                       help = 'maximum number of alias checks to add',
                       type = int)
    parser.add_argument('--inline',
                        help = 'whether to inline AssertNoAlias',
                        action = 'store_true',
                        default = False)
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # Some AAs require additional plugins.
    # TODO: Should be specified in a configuration file.
    if args.aa == 'ds-aa':
        cmd = rcs_utils.load_plugin(cmd, 'LLVMDataStructure')
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
    if args.max_alias_checks is not None:
        cmd = string.join((cmd, '-max-alias-checks', str(args.max_alias_checks)))
    assert args.bc.endswith('.bc')
    bc_with_alias_checker = args.bc[0:-3] + '.alias_checker.bc'
    cmd = string.join((cmd,
                       '-o', bc_with_alias_checker,
                       '<', args.bc))
    rcs_utils.invoke(cmd)

    if args.inline:
        cmd = dynaa_utils.load_all_plugins('opt')
        cmd = string.join((cmd, '-inline-alias-checker'))
        bc_inlined = args.bc[0:-3] + '.inlined.bc'
        cmd = string.join((cmd,
                           '-o', bc_inlined,
                           '<', bc_with_alias_checker))
        rcs_utils.invoke(cmd)

    cmd = string.join(('clang++', '-O3'))
    if args.inline:
        cmd = string.join((cmd, bc_inlined))
    else:
        cmd = string.join((cmd,
                           bc_with_alias_checker,
                           rcs_utils.get_libdir() + '/libDynAAAliasChecker.a'))
    cmd = string.join((cmd,
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
