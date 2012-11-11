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

    os.system(base_cmd + ' ../ ../../rcs/lib/CFG ../../rcs/lib/ID ../../rcs/lib/SourceLocator')
    os.system(base_cmd + ' ../lib/Instrumenters')
    os.system(base_cmd + ' ../runtime')
    os.system(base_cmd + ' ../lib/Analyses ../lib/Checkers')
