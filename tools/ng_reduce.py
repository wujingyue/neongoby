#!/usr/bin/env python

import argparse
import rcs_utils
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Reduce testcase for alias pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('logs', nargs='+', help = 'point-to logs (.pts)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                        str(ng_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = ng_utils.get_aa_choices())
    parser.add_argument('vid1', help = 'ValueID of Pointer 1')
    parser.add_argument('vid2', help = 'ValueID of Pointer 2')
    args = parser.parse_args()

    cmd = ng_utils.load_all_plugins('opt')
    # reducer need be put before aa
    cmd = ' '.join((cmd, '-remove-untouched-code'))
    cmd = ' '.join((cmd, '-simplifycfg'))

    # Load the checked AA
    cmd = ng_utils.load_aa(cmd, args.aa)

    cmd = ' '.join((cmd, '-verify-reducer'))
    cmd = ' '.join((cmd, '-strip'))
    for log in args.logs:
        cmd = ' '.join((cmd, '-log-file', log))
    cmd = ' '.join((cmd, '-pointer-value', args.vid1))
    cmd = ' '.join((cmd, '-pointer-value', args.vid2))
    cmd = ' '.join((cmd, '-o', args.prog + '.reduce.bc'))
    cmd = ' '.join((cmd, '<', args.prog + '.bc'))
    rcs_utils.invoke(cmd)

    cmd = ' '.join(('clang++', args.prog + '.reduce.bc',
                    '-o', args.prog + '.reduce'))
    linking_flags = rcs_utils.get_linking_flags(args.prog)
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    rcs_utils.invoke(cmd)
