#!/usr/bin/env python

import os
import sys
import string
import rcs_utils
import dynaa_utils

if __name__ == '__main__':
    assert len(sys.argv) == 2
    prog_name = sys.argv[1]
    instrumented_bc = prog_name + '.inst.bc'
    instrumented_exe = prog_name + '.inst'

    cmd = dynaa_utils.load_all_plugins('opt')
    cmd = string.join((cmd, '-instrument-memory'))
    cmd = string.join((cmd, '-o', instrumented_bc))
    cmd = string.join((cmd, '<', prog_name + '.bc'))
    rcs_utils.invoke(cmd)

    cmd = string.join(('clang++', instrumented_bc,
                       rcs_utils.get_libdir() + '/libDynAAMemoryHooks.a',
                       '-o', instrumented_exe, '-pthread'))
    if prog_name.startswith('pbzip2'):
        cmd = string.join((cmd, '-lbz2'))
    if prog_name.startswith('ferret'):
        cmd = string.join((cmd, '-lgsl', '-lblas'))
    if prog_name.startswith('gpasswd'):
        cmd = string.join((cmd, '-lcrypt'))
    if prog_name.startswith('cvs'):
        cmd = string.join((cmd, '-lcrypt'))
    rcs_utils.invoke(cmd)
