#!/usr/bin/env python

import argparse
import os
import sys
import re
import multiprocessing

# TODO: don't hardcode the path
CSMITH_HOME = '/home/jingyue/Research/csmith-2.1.0'

def run_neongoby_offline(run_id):
    print >> sys.stderr, 'Run %d' % run_id
    working_dir = '%s/csmith-%d' % (workspace, run_id)
    os.system('rm -rf %s' % working_dir)
    os.makedirs(working_dir)
    old_working_dir = os.getcwd()
    os.chdir(working_dir)
 
    os.system('%s/src/csmith > test.c 2>> stderr' % (CSMITH_HOME))
    os.system('clang test.c -o test.bc -c -emit-llvm -I%s/runtime 2>> stderr' % CSMITH_HOME)
    os.system('ng_offline.py test basicaa --all --time-limit 5 >> stderr 2>&1')

    stderr = open('stderr', 'r')
    for line in stderr:
        if line.find('Detected') >= 0:
            stderr.close()
            return False
    stderr.close()

    # If NeonGoby didn't detect any error, remove the working directory to save
    # space.
    os.chdir(old_working_dir)
    os.system('rm -rf %s' % working_dir)
    return True

def worker_process(base_run_id):
    run_id = base_run_id
    while True:
        if not run_neongoby_offline(run_id):
            print 'Run %d is problematic' % run_id
            break
        run_id += n_processes

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='CSmith driver')
    parser.add_argument('-p', type=int, default=1,
                        help='# of threads (default: 1)')
    parser.add_argument('-d', default='/tmp', help='workspace (default: /tmp)')
    args = parser.parse_args()

    global n_processes
    n_processes = args.p
    global workspace
    workspace = args.d

    children = []
    for i in range(n_processes):
        child = multiprocessing.Process(target=worker_process, args=(i,))
        children.append(child)
        child.start()
    for child in children:
        child.join()
