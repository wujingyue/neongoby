#!/usr/bin/env python

import argparse
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Reduce testcase for alias pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('log', help = 'the point-to log (.pts)')
    parser.add_argument('vid1', help = 'ValueID of Pointer 1')
    parser.add_argument('vid2', help = 'ValueID of Pointer 2')
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')

    cmd = ' '.join(cmd, '-reduce-testcase')
    cmd = ' '.join(cmd, '-log-file', args.log)
    cmd = ' '.join(cmd, '-pointer-value', args.vid1)
    cmd = ' '.join(cmd, '-pointer-value', args.vid2)
    cmd = ' '.join(cmd, '-o', args.prog + '.reduce.bc')
    cmd = ' '.join(cmd, '<', args.prog + '.bc')
    rcs_utils.invoke(cmd)

    cmd = ' '.join('chmod 775 ', args.prog + '.reduce.bc')
