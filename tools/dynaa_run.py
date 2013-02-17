#!/usr/bin/env python

import time
import os
import sys
import rcs_utils

if __name__ == '__main__':
    lt = time.localtime()
    log_dir = '/tmp/ng-%04d%02d%02d-%02d%02d%02d' % (lt.tm_year, lt.tm_mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec)
    if os.path.exists(log_dir):
        print >> sys.stderr, 'The target directory already exists.'
        sys.exit(1)
    os.makedirs(log_dir)
    # TODO: unescape
    rcs_utils.invoke(' '.join(['LOG_DIR=' + log_dir] + sys.argv[1:]))
