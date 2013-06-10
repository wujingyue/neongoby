#!/usr/bin/env python

import argparse
import os
import sys
import string
import rcs_utils
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Find out the trace of ' + \
            'two pointers who alias in real execution')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log (.pts)')
    parser.add_argument('id1', help = 'RecordID/ValueID of Pointer 1')
    parser.add_argument('id2', help = 'RecordID/ValueID of Pointer 2')
    parser.add_argument('--value',
                        help = 'the two IDs specified are value IDs',
                        action = 'store_true',
                        default = False)
    args = parser.parse_args()

    cmd = ng_utils.load_all_plugins('opt')

    cmd = string.join((cmd, '-slice-trace'))
    cmd = string.join((cmd, '-log-file', args.log))
    if args.value:
        cmd = string.join((cmd, '-starting-value', args.id1))
        cmd = string.join((cmd, '-starting-value', args.id2))
    else:
        cmd = string.join((cmd, '-starting-record', args.id1))
        cmd = string.join((cmd, '-starting-record', args.id2))
    cmd = string.join((cmd, '-analyze', '<', args.bc))

    rcs_utils.invoke(cmd)
