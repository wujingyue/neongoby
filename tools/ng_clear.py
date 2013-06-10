#!/usr/bin/env python

import argparse
import os

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Remove the log files')
    parser.add_argument('dir',
                        help = 'the containing directory of the log files ' + \
                                '(/tmp by default)',
                        nargs = '?',
                        default = '/tmp')
    args = parser.parse_args()

    os.system('rm ' + args.dir + '/pts-*')
    os.system('rm ' + args.dir + '/report-*')
