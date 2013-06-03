#!/usr/bin/env python

import argparse
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Reduce testcase for alias pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('logs', nargs='+', help = 'point-to logs (.pts)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                        str(dynaa_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = dynaa_utils.get_aa_choices())
    parser.add_argument('vid1', help = 'ValueID of Pointer 1')
    parser.add_argument('vid2', help = 'ValueID of Pointer 2')
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('dynaa_opt')
    # cmd = ' '.join((cmd, '-debug-pass=Details'))

    # slice trace and add tags for reducer
    cmd = ' '.join((cmd, '-slice-for-reduction'))
    cmd = ' '.join((cmd, '-starting-value', args.vid1))
    cmd = ' '.join((cmd, '-starting-value', args.vid2))
    for log in args.logs:
        cmd = ' '.join((cmd, '-log-file', log))

    # Load the checked AA
    cmd = dynaa_utils.load_aa(cmd, args.aa)

    cmd = ' '.join((cmd, '>', args.prog + '.reduce.big.bc'))
    cmd = ' '.join((cmd, '<', args.prog + '.bc'))
    rcs_utils.invoke(cmd)

    # simplifycfg and strip
    cmd = dynaa_utils.load_all_plugins('opt')
    cmd = ' '.join((cmd, '-simplifycfg'))
    cmd = ' '.join((cmd, '-strip'))
    cmd = ' '.join((cmd, '-verify'))
    cmd = ' '.join((cmd, '>', args.prog + '.reduce.bc'))
    cmd = ' '.join((cmd, '<', args.prog + '.reduce.big.bc'))
    rcs_utils.invoke(cmd)

    # reducer may lead to linking errors
    cmd = ' '.join(('clang++', args.prog + '.reduce.bc',
                    '-o', args.prog + '.reduce'))
    linking_flags = rcs_utils.get_linking_flags(args.prog)
    cmd = ' '.join((cmd, ' '.join(linking_flags)))
    # rcs_utils.invoke(cmd)

    cmd = ' '.join(('llvm-dis', args.prog + '.reduce.bc'))
    rcs_utils.invoke(cmd)
