#!/usr/bin/env python

import argparse
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Reduce testcase for alias pointers')
    parser.add_argument('prog', help = 'the program name (e.g. mysqld)')
    parser.add_argument('logs', nargs='+', help = 'the point-to log (.pts)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis: ' + \
                        str(dynaa_utils.get_aa_choices()),
                        metavar = 'aa',
                        choices = dynaa_utils.get_aa_choices())
    parser.add_argument('vid1', help = 'ValueID of Pointer 1')
    parser.add_argument('vid2', help = 'ValueID of Pointer 2')
    # Due to the behavior of LLVM's alias analysis chaining, the baseline AA
    # must be an ImmutablePass.
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be ' + \
                        'correct: ' + str(dynaa_utils.get_aa_choices()),
                        metavar = 'baseline_aa',
                        default = 'no-aa',
                        choices = ['no-aa', 'basicaa', 'tbaa'])
    args = parser.parse_args()

    cmd = dynaa_utils.load_all_plugins('opt')
    # reducer need be put before aa
    cmd = ' '.join((cmd, '-remove-untouched-code'))
    cmd = ' '.join((cmd, '-simplifycfg'))

    # Load the baseline AA
    if args.baseline == args.aa:
      sys.stderr.write('\033[0;31m')
      print >> sys.stderr, 'Error: Baseline and the checked AA',
      print >> sys.stderr, 'must be different'
      sys.stderr.write('\033[m')
      sys.exit(1)

    # baseline need be put before aa
    cmd = dynaa_utils.load_aa(cmd, args.baseline)
    cmd = ' '.join((cmd, '-baseline-aa'))
    cmd = ' '.join((cmd, '-baseline-aa-name', args.baseline))

    # Load the checked AA
    cmd = dynaa_utils.load_aa(cmd, args.aa)

    # Some AAs don't support inter-procedural alias queries.
    # Add -baseline-intra option for them.
    if dynaa_utils.supports_intra_proc_queries_only(args.baseline):
      cmd = ' '.join((cmd, '-baseline-intra'))

    cmd = ' '.join((cmd, '-verify-reducer'))
    cmd = ' '.join((cmd, '-strip'))
    for index, log in enumerate(args.logs):
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
