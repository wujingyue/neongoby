#!/usr/bin/env python

# Author: Jingyue

import argparse
import os
import sys
import string

def load_plugin(cmd, plugin):
    return ' '.join((cmd, '-load $LLVM_ROOT/install/lib/' + plugin + '.so'))

def get_base_cmd(args):
    base_cmd = 'opt'
    base_cmd = load_plugin(base_cmd, 'ID')
    base_cmd = load_plugin(base_cmd, 'PointerAnalysis')
    base_cmd = load_plugin(base_cmd, 'DynamicAnalyses')
    base_cmd = load_plugin(base_cmd, 'Checkers')
    return base_cmd

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description = 'Check the soundness of the specified AA')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log')
    aa_choices = ['tbaa', 'basicaa', 'no-aa', 'ds-aa', 'anders-aa', 'bc2bdd-aa']
    parser.add_argument('aa',
            help = 'the checked alias analysis: ' + str(aa_choices),
            metavar = 'aa',
            choices = aa_choices)
    args = parser.parse_args()

    cmd = get_base_cmd(args)
    # Some AAs require additional plugins.
    # TODO: Should be specified in a configuration file.
    if args.aa == 'ds-aa':
        cmd = load_plugin(cmd, 'LLVMDataStructure')
        cmd = ' '.join((cmd, '-intra'))
    elif args.aa == 'basicaa':
        cmd = ' '.join((cmd, '-intra'))
    elif args.aa == 'anders-aa':
        cmd = load_plugin(cmd, 'Andersens')
    elif args.aa == 'bc2bdd-aa':
        if not os.path.exists('bc2bdd.conf'):
            sys.stderr.write('\033[1;31m')
            print >> sys.stderr, 'Error: bc2bdd-aa requires bc2bdd.conf, ' \
                    'which cannot be found in the current directory.'
            sys.stderr.write('\033[m')
            sys.exit(1)
        cmd = load_plugin(cmd, 'bc2bdd')

    cmd = ' '.join((cmd, '-' + args.aa))
    cmd = ' '.join((cmd, '-check-aa'))
    cmd = ' '.join((cmd, '-log-file', args.log))
    cmd = ' '.join((cmd, '-disable-output', '<', args.bc))

    ret = os.system(cmd)
    sys.exit(ret)
