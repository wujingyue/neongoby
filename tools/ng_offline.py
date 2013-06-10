#!/usr/bin/env python

# Runs neongoby on a standalone program.
# Instruments the program, runs the instrumented program, and detects alias
# analysis errors offline in this single script.

import argparse
import rcs_utils
import ng_utils
import os
import re
import sys

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Run the offline mode')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                                str(ng_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = ng_utils.get_aa_choices())
    parser.add_argument('--all',
                        help = 'hook and check all pointers (False by default)',
                        action = 'store_true',
                        default = False)
    # suited for csmith. The generated program is sometimes too large and takes
    # forever to run.
    parser.add_argument('--time-limit',
                        help = 'time limit for the program (in seconds)',
                        type = int)
    parser.add_argument('--dir',
                        help = 'where to put the log files (default: /tmp)',
                        type = str,
                        default = '/tmp')
    args = parser.parse_args()

    # ng_hook_mem
    cmd = ' '.join(('ng_hook_mem.py', args.prog))
    if args.all:
        cmd = ' '.join((cmd, '--hook-all'))
    rcs_utils.invoke(cmd)

    # run the instrumented program
    cmd = './' + args.prog + '.inst'
    if args.time_limit is not None:
        cmd = ' '.join(('timeout', str(args.time_limit), cmd))
    cmd = ' '.join(('LOG_DIR=' + args.dir, cmd))
    timeout = rcs_utils.invoke(cmd, False)
    # FIXME: could fail for other reasons
    if timeout != 0:
        print >> sys.stderr, 'Warning: runtime error or time limit exceeded'

    # ng_check_aa.py
    # use automatic globbing
    cmd = ' '.join(('ng_check_aa.py',
                    args.prog + '.bc',
                    args.dir + '/pts-*',
                    args.aa))
    if args.all:
        cmd = ' '.join((cmd, '--check-all'))
    rcs_utils.invoke(cmd)
