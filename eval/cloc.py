#!/usr/bin/env python

import os

if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.realpath(__file__)))

    base_cmd = 'cloc --exclude-lang='
    base_cmd += 'Python,'
    base_cmd += 'D,'
    base_cmd += 'Assembly,'
    base_cmd += 'HTML,'
    base_cmd += 'make,'
    base_cmd += 'm4,'
    base_cmd += '"Bourne Shell",'
    base_cmd += '"Bourne Again Shell"'

    print '=== All ==='
    os.system(base_cmd + \
            ' ../ ../../rcs/lib/CFG' + \
            ' ../../rcs/lib/ID' + \
            ' ../../rcs/lib/SourceLocator')

    print '=== Instrumenter ==='
    os.system(base_cmd + \
            ' ../lib/Instrumenters/MemoryInstrumenter.cpp' + \
            ' ../lib/Instrumenters/Preparer.cpp')

    print '=== Logger ==='
    os.system(base_cmd + ' ../runtime/MemoryHooks')

    print '=== Detector ==='
    os.system(base_cmd + ' ../lib/Analyses ../lib/Checkers')

    print '=== Online Mode ==='
    os.system(base_cmd + \
            ' ../lib/Instrumenters/AliasCheckerInstrumenter.cpp' + \
            ' ../lib/Instrumenters/AliasCheckerInliner.cpp' + \
            ' ../runtime/AliasChecker')
