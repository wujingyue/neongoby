#!/usr/bin/env python

# Runs neongoby on a standalone program.
# Instruments the program, runs the instrumented program, and detects alias
# analysis errors offline in this single script.

import argparse
import rcs_utils
import dynaa_utils
import os
import re
import sys

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Run the offline mode')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                                str(dynaa_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = dynaa_utils.get_aa_choices())
    parser.add_argument('--all',
                        help = 'hook and check all pointers (False by default)',
                        action = 'store_true',
                        default = False)
    # suited for csmith. The generated program is sometimes too large and takes
    # forever to run.
    parser.add_argument('--time-limit',
                        help = 'time limit for the program (in seconds)',
                        type = int)
    args = parser.parse_args()

    # dynaa_hook_mem
    cmd = ' '.join(('dynaa_hook_mem.py', args.prog))
    if args.all:
        cmd = ' '.join((cmd, '--hook-all'))
    rcs_utils.invoke(cmd)

    # run the instrumented program
    cmd = './' + args.prog + '.inst'
    if args.time_limit is not None:
        cmd = ' '.join(('timeout', str(args.time_limit), cmd))
    rcs_utils.invoke(cmd, False)

    # move the log to the current directory
    dir_name = '/tmp'
    the_log_file = ''
    n_log_files = 0
    max_modified_time = 0
    for file_name in os.listdir(dir_name):
        if re.match('pts-\\d+', file_name):
            n_log_files += 1
            full_path = os.path.join(dir_name, file_name)
            modified_time = os.stat(full_path).st_mtime
            if modified_time > max_modified_time:
                max_modified_time = modified_time
                the_log_file = full_path
    assert n_log_files != 0, 'cannot find any log file under ' + dir_name
    if n_log_files > 1:
        print >> sys.stderr, 'Warning: multiple log files. ' + \
                'Pick the latest modified one.'

    # dynaa_check_aa.py
    cmd = ' '.join(('dynaa_check_aa.py',
                    args.prog + '.bc',
                    the_log_file,
                    args.aa))
    if args.all:
        cmd = ' '.join((cmd, '--check-all'))
    ret = rcs_utils.invoke(cmd, False)

    # dynaa_clear.py
    # clear other log files
    # The script exits with the exit code of dynaa_check_aa.py.
    # Therefore, we use False here to avoid overwriting the exit code.
    rcs_utils.invoke('dynaa_clear.py', False)

    sys.exit(ret)
