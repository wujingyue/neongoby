#!/usr/bin/env python

# Author: Junyang

import argparse
import os
import sys
import string
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Find out the trace of ' + \
            'two pointers who alias in real execution')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('log', help = 'the point-to log (.pts)')
    parser.add_argument('record1', help = 'RecordID of the first pointer')
    parser.add_argument('record2', help = 'RecordID of the second pointer')
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')

    cmd = string.join((cmd, '-slice-trace'))
    cmd = string.join((cmd, '-log-file', args.log))
    cmd = string.join((cmd, '-record', args.record1))
    cmd = string.join((cmd, '-record', args.record2))
    cmd = string.join((cmd, '-analyze', '<', args.bc))

    rcs_utils.invoke(cmd)
