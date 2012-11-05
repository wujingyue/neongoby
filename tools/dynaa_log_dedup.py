#!/usr/bin/env python

import argparse
import logging
import re
import sys

parser = argparse.ArgumentParser()
parser.add_argument('input_files', metavar='<input file>', type=str, nargs='+',
    help='log file to be analyzed')

args = parser.parse_args()
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger('aa_analyze_log')
logger.setLevel(logging.DEBUG)

missing_reg = re.compile('.*Missing alias:.*')
ptrinfo_reg = re.compile('.*\[(\d+)\](.*)$')

for input_file in args.input_files:
    logger.info('Processing %s' % input_file)
    value_desc_db = {}
    missing_db = set()
    with open(input_file, 'r') as logf:
        while True:
            line = logf.readline()
            if (line == ''): break
            match_ret = missing_reg.match(line)
            if (match_ret):
                got_ptr = 0
                ptrinfo = []
                while (True):
                    newline = logf.readline()
                    if (newline == ''):
                        logger.error('Unexpected end of file reached.')
                        sys.exit(1)
                    ptr_ret = ptrinfo_reg.match(newline)
                    if (ptr_ret):
                        got_ptr+=1

                        result = {}
                        value_id = int(ptr_ret.group(1))
                        value_desc = ptr_ret.group(2)
                        value_desc_db[value_id] = value_desc
                        ptrinfo += [value_id]

                        if (got_ptr == 2):
                            break

#                print "Missing alias:", ptrinfo
                missing_db.add(frozenset(ptrinfo))

        for pair in missing_db:
            print "Missing alias:"
            if (len(pair) == 2):
                for elem in pair:
                    print "[%d]%s" % (elem, value_desc_db[elem])
            else:
                for elem in pair:
                    print "[%d]%s" % (elem, value_desc_db[elem])
                    print "[%d]%s" % (elem, value_desc_db[elem])
        print "Total missing alias pairs: %d" % (len(missing_db))


