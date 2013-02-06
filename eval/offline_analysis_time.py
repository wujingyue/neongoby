#!/usr/bin/env python

import re
import sys

if __name__ == '__main__':
    total_time = 0
    for line in sys.stdin:
        what = re.search('([\\d.]+)user ([\\d.]+)system', line)
        if what is not None:
            time_this_run = float(what.group(1)) + float(what.group(2))
            sys.stderr.write(line)
            total_time += time_this_run
    print
    print 'offline analysis time:', total_time
