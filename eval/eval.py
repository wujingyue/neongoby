#!/usr/bin/python

import os
import shutil
import string
import subprocess
import sys
import time

APPS = ['httpd', 'mysql']
AAS = ['basicaa', 'anders-aa', 'ds-aa']

APPS_DIR = os.getenv('APPS_DIR')


def invoke(args, out_file=None, redirect_stderr=False, is_mysqld=False):
    sys.stdout.write('\033[0;32m%s\033[m\n' % string.join(args))
    if is_mysqld:
        p = subprocess.Popen(args)
    else:
        if out_file is None:
            subprocess.check_call(args)
        else:
            if redirect_stderr:
                out = subprocess.check_output(args, stderr=subprocess.STDOUT)
            else:
                out = subprocess.check_output(args)
            out_file.write(out)
            sys.stdout.write(out)


def get_pts_files():
    for filename in os.listdir('/mnt/sdc/dyn-aa'):
        if filename.startswith('pts'):
            yield os.path.join('/mnt/sdc/dyn-aa', filename)


def run_httpd(executable, out_file, threads='1', report=False):
    cpu0 = '0'
    cpu1 = '1'
    if threads == '4':
        cpu0 = '0,2,4,6'
        cpu1 = '1,3,5,7'
    if report:
        invoke(['taskset', '-c', cpu0,
                './' + executable, '-c', 'MaxClients 25'],
               out_file, redirect_stderr=True)
        out_file = None
    else:
        invoke(['taskset', '-c', cpu0, './' + executable])
    time.sleep(3)
    invoke(['time', 'taskset', '-c', cpu1,
            os.path.join(APPS_DIR, 'apache/run-ab'),
            '--times', '10', '10000', threads, 'localhost:8000/test.html'],
           out_file)
    invoke(['./httpd', '-k', 'stop'])
    time.sleep(5)


def run_mysql(executable, out_file, threads='1', small_workload=False):
    cpu0 = '0'
    cpu1 = '1'
    if threads == '4':
        cpu0 = '0,2,4,6'
        cpu1 = '1,3,5,7'
    invoke(['taskset', '-c', cpu0, './' + executable, '--thread_stack=8388608'],
           is_mysqld=True)
    time.sleep(3)
    args = ['taskset', '-c', cpu1, os.path.join(APPS_DIR, 'mysql/run-sysbench')]
    if small_workload:
        args += ['-n', '100', '-r', '1']
    else:
        args += ['-n', '10000', '-r', '10']
    if threads == '4':
        args += ['-p', threads]
    invoke(args, out_file)
    invoke(['killall', executable])
    time.sleep(3)


def eval_baseline_httpd(threads='1'):
    os.chdir('httpd')
    invoke(['clang', '-pthread', '-lcrypt', '-o', 'httpd', 'httpd.bc'])
    with open('../baseline-httpd-%s.out' % threads, 'w') as out_file:
        run_httpd('httpd', out_file, threads)
    os.chdir('..')


def eval_baseline_mysql(threads='1'):
    os.chdir('mysql')
    invoke(['clang++', '-pthread', '-lcrypt', '-ldl', '-lz', '-o', 'mysqld',
            'mysqld.bc'])
    with open('../baseline-mysql-%s.out' % threads, 'w') as out_file:
        run_mysql('mysqld', out_file, threads)
    os.chdir('..')


def eval_online_httpd(threads='1'):
    os.chdir('httpd')
    for aa in AAS:
        args = ['ng_insert_alias_checker.py', 'httpd', aa]
        # with open('../online-httpd-%s-vr.out' % aa, 'w') as out_file:
        #     invoke(args, out_file)
        #     run_httpd('httpd.ac', out_file)
        # with open('../online-httpd-%s-sl.out' % aa, 'w') as out_file:
        #     invoke(args + ['--no-phi'], out_file)
        #     run_httpd('httpd.ac', out_file)

        if aa == 'basicaa':
            with open('../online-httpd-basicaa-deref-%s.out' % threads,
                      'w') as out_file:
                invoke(args, out_file)
                run_httpd('httpd.ac', out_file, threads)
        else:
            with open('../online-httpd-%s-delta-deref-%s.out' % (aa, threads),
                      'w') as out_file:
                invoke(args + ['--baseline', 'basicaa'], out_file)
                run_httpd('httpd.ac', out_file, threads)
        continue
        if aa == 'anders-aa':
            with open('../online-httpd-anders-aa-%s.out' % threads,
                      'w') as out_file:
                invoke(args + ['--check-all'], out_file)
                run_httpd('httpd.ac', out_file, threads)
            with open('../online-httpd-anders-aa-delta-%s.out' % threads,
                      'w') as out_file:
                invoke(args + ['--baseline', 'basicaa', '--check-all'],
                       out_file)
                run_httpd('httpd.ac', out_file, threads)
            # # with open('../online-httpd-anders-aa-deref-%s.out' % threads,
            # #           'w') as out_file:
            # #     invoke(args, out_file)
            # #     run_httpd('httpd.ac', out_file, threads)
            # with open('../online-httpd-anders-aa-report-%s.out' % threads,
            #           'w') as out_file:
            #     invoke(args + ['--check-all', '--action-if-missed', 'report'])
            #     run_httpd('httpd.ac', out_file, threads, report=True)
            #     shutil.move('apache-install/logs/error_log',
            #                 ('../online-httpd-anders-aa-report-%s.error_log' %
            #                  threads))
            # with open('../online-httpd-anders-aa-delta-report-%s.out' % threads,
            #           'w') as out_file:
            #     invoke(args + ['--baseline', 'basicaa', '--check-all',
            #                    '--action-if-missed', 'report'])
            #     run_httpd('httpd.ac', out_file, threads, report=True)
            #     shutil.move('apache-install/logs/error_log',
            #                 ('../online-httpd-anders-aa-delta-report-%s.error_log' %
            #                  threads))
            # # with open('../online-httpd-anders-aa-deref-report-%s.out' % threads,
            # #           'w') as out_file:
            # #     invoke(args + ['--action-if-missed', 'report'])
            # #     run_httpd('httpd.ac', out_file, threads, report=True)
            # #     shutil.move('apache-install/logs/error_log',
            # #                 ('../online-httpd-anders-aa-deref-report-%s.error_log'
            # #                  % threads))
            # with open(('../online-httpd-anders-aa-delta-deref-report-%s.out' %
            #            threads), 'w') as out_file:
            #     invoke(args + ['--baseline', 'basicaa',
            #                    '--action-if-missed', 'report'])
            #     run_httpd('httpd.ac', out_file, threads, report=True)
            #     shutil.move('apache-install/logs/error_log',
            #                 ('../online-httpd-anders-aa-delta-deref-report-%s.error_log' %
            #                  threads))
    os.chdir('..')


def eval_online_mysql(threads='1'):
    os.chdir('mysql')
    for aa in AAS:
        args = ['ng_insert_alias_checker.py', 'mysqld', aa]
        # if aa == 'ds-aa':
        #     args.append('--skip-huge-functions')
        # with open('../online-mysql-%s-vr.out' % aa, 'w') as out_file:
        #     invoke(args, out_file)
        #     run_mysql('mysqld.ac', out_file)
        # with open('../online-mysql-%s-sl.out' % aa, 'w') as out_file:
        #     invoke(args + ['--no-phi'], out_file)
        #     run_mysql('mysqld.ac', out_file)
        if aa in ['ds-aa', 'anders-aa']:
            with open('../hybrid-mysql-%s-delta-deref-%s.out' % (aa, threads),
                      'w') as out_file:
                invoke(['ng_hybrid.py', 'mysqld', aa,
                        '--baseline', 'basicaa',
                        '--offline-funcs', '_Z10MYSQLparsePv',],
                       out_file)
                run_mysql('mysqld.hybrid', out_file, threads)
                pts_files = list(get_pts_files())
                invoke(['time', 'ng_check_aa.py', '--disable-print-value',
                        'mysqld.bc'] + pts_files + [aa],
                       out_file, redirect_stderr=True)
            for pts_file in pts_files:
                shutil.move(pts_file, '/mnt/sdc/dyn-aa/backup')
        elif aa == 'basicaa':
            with open('../hybrid-mysql-%s-deref-%s.out' % (aa, threads),
                      'w') as out_file:
                invoke(['ng_hybrid.py', 'mysqld', aa,
                        '--offline-funcs', '_Z10MYSQLparsePv',],
                       out_file)
                run_mysql('mysqld.hybrid', out_file, threads)
                pts_files = list(get_pts_files())
                invoke(['time', 'ng_check_aa.py', '--disable-print-value',
                        'mysqld.bc'] + pts_files + [aa],
                       out_file, redirect_stderr=True)
            for pts_file in pts_files:
                shutil.move(pts_file, '/mnt/sdc/dyn-aa/backup')
        elif aa == 'basicaa':
            with open('../online-mysql-basicaa-deref-%s.out' % threads,
                      'w') as out_file:
                invoke(args, out_file)
                run_mysql('mysqld.ac', out_file, threads)
        else:
            with open('../online-mysql-%s-delta-deref-%s.out' % (aa, threads),
                      'w') as out_file:
                invoke(args + ['--baseline', 'basicaa'], out_file)
                run_mysql('mysqld.ac', out_file, threads)
    os.chdir('..')


def eval_offline_httpd(threads='1'):
    os.chdir('httpd')
    with open('../offline-httpd-%s.out' % threads, 'w') as out_file:
        invoke(['time', 'ng_hook_mem.py',
                '--hook-all', 'httpd'],
               out_file)
        run_httpd('httpd.inst', out_file, threads)
    pts_files = list(get_pts_files())
    for aa in AAS:
        with open('../offline-httpd-%s-%s.out' % (aa, threads),
                  'w') as out_file:
            invoke(['time', 'ng_check_aa.py', '--check-all',
                    '--disable-print-value', 'httpd.bc'] + pts_files + [aa],
                   out_file, redirect_stderr=True)
        if aa == 'anders-aa':
            with open('../offline-httpd-anders-aa-delta-%s.out' % threads,
                      'w') as out_file:
                invoke(['time', 'ng_check_aa.py', '--check-all',
                        '--baseline', 'basicaa', '--disable-print-value',
                        'httpd.bc'] + pts_files + [aa],
                       out_file, redirect_stderr=True)
    for pts_file in pts_files:
        shutil.move(pts_file, '/mnt/sdc/dyn-aa/backup')
    if 'anders-aa' in AAS:
        with open('../offline-httpd-deref-%s.out' % threads,
                  'w') as out_file:
            invoke(['time', 'ng_hook_mem.py', 'httpd'], out_file)
            run_httpd('httpd.inst', out_file, threads)
        pts_files = list(get_pts_files())
        # with open('../offline-httpd-anders-aa-deref-%s.out' % threads,
        #           'w') as out_file:
        #     for pts_file in pts_files:
        #         out_file.write(pts_file + '\n')
        #         invoke(['time', 'ng_check_aa.py', '--disable-print-value',
        #                 'httpd.bc', pts_file, aa],
        #                out_file, redirect_stderr=True)
        with open('../offline-httpd-anders-aa-delta-deref-%s.out' % threads,
                  'w') as out_file:
            invoke(['time', 'ng_check_aa.py',
                    '--baseline', 'basicaa',
                    '--disable-print-value',
                    'httpd.bc'] + pts_files + [aa],
                   out_file, redirect_stderr=True)
        for pts_file in pts_files:
            shutil.move(pts_file, '/mnt/sdc/dyn-aa/backup')
    os.chdir('..')


def eval_offline_mysql(threads='1'):
    os.chdir('mysql')
    with open('../offline-mysql-%s.out' % threads, 'w') as out_file:
        invoke(['ng_hook_mem.py', '--hook-all', 'mysqld'], out_file)
        run_mysql('mysqld.inst', out_file, threads, small_workload=True)
    pts_files = list(get_pts_files())
    for aa in AAS:
        with open('../offline-mysql-%s-%s.out' % (aa, threads),
                  'w') as out_file:
            invoke(['time', 'ng_check_aa.py', '--check-all',
                    '--disable-print-value', 'mysqld.bc'] + pts_files + [aa],
                   out_file, redirect_stderr=True)
    for pts_file in pts_files:
        shutil.move(pts_file, '/mnt/sdc/dyn-aa/backup')
    os.chdir('..')


def main():
    import argparse
    global APPS, AAS
    parser = argparse.ArgumentParser(
        description=('\033[0;31mnote: before running this script, please '
                     'make sure that no httpd/mysqld instance is running, '
                     'and please remove apache-install/logs/error_log and '
                     'all existing /mnt/sdc/dyn-aa/pts* files.\033[m'))
    parser.add_argument('--app', choices=APPS,
                        help='only eval the specified application')
    parser.add_argument('--aa', choices=AAS,
                        help='only eval the specified AA')
    parser.add_argument('--threads', choices=['1', '4'], default='4',
                        help='number of threads')
    parser.add_argument('--no-eval-baseline', action='store_true',
                        help='do not eval the baseline')
    parser.add_argument('--no-eval-online', action='store_true',
                        help='do not eval the online mode')
    parser.add_argument('--no-eval-offline', action='store_true',
                        help='do not eval the offline mode')
    args = parser.parse_args()
    os.putenv('LOG_DIR', '/mnt/sdc/dyn-aa')
    if args.app is not None:
        APPS = [args.app]
    if args.aa is not None:
        AAS = [args.aa]
    for app in APPS:
        if not args.no_eval_baseline:
            globals()['eval_baseline_' + app](args.threads)
        if not args.no_eval_online:
            globals()['eval_online_' + app](args.threads)
        if not args.no_eval_offline:
            globals()['eval_offline_' + app](args.threads)


if __name__ == '__main__':
    main()
