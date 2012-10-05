#!/usr/bin/env python

# Author: Jingyue

import os

def ok(max_alias_checks):
    os.system('dynaa_insert_alias_checker.py mysqld.bc basicaa --max-alias-checks ' + str(max_alias_checks))
    os.system('./mysqld.alias_checker &')
    os.system('sleep 1')
    ret = os.system("$HOME/Research/apps/mysql/mysql-install/bin/mysql -u root -e 'show databases'")
    os.system('killall mysqld.alias_checker')
    os.system('sleep 1')
    return ret == 0

if __name__ == '__main__':
    low = 0
    high = 1000000
    while low < high:
        mid = (low + high) / 2
        if ok(mid):
            low = mid + 1
        else:
            high = mid
    assert low == high
    print 'Error when max-alias-checks =', low
