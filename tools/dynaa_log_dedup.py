#!/usr/bin/env python

import argparse
import logging
import re
import sys

parser = argparse.ArgumentParser()
parser.add_argument('input_files', metavar='<input file>', type=str, nargs='+',
    help='log file to be analyzed')
parser.add_argument('--count-interp-ptr', help = 'count inter-procedure pointer',
        default = False, action = 'store_true')

args = parser.parse_args()
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger('aa_analyze_log')
logger.setLevel(logging.DEBUG)

missing_reg = re.compile('.*Missing alias:.*')
deref_reg = re.compile('.*Missing alias:.*\(deref\).*')
intra_reg = re.compile('.*Missing alias:.*\(intra\).*')
ptrinfo_reg = re.compile('.*\[(\d+)\](.*)$')

ptrinfo_func_reg = re.compile('.*\[(\d+)\]\s+([^ ]+):\s+([^ ]+)\s+=\s+(.*)$')
GLOBAL_ID = 0
both_global_count = 0
one_global_count = 0
same_func_count = 0
diff_func_count = 0
intra_deref= 0
inter_deref= 0
intra_nonderef= 0
inter_nonderef= 0

for input_file in args.input_files:
    logger.info('Processing %s' % input_file)
    value_desc_db = {}
    missing_db = set()
    missing_detail_db = {}
    with open(input_file, 'r') as logf:
        while True:
            line = logf.readline()
            if (line == ''): break
            match_ret = missing_reg.match(line)
            if (match_ret):
                if deref_reg.match(line):
                    deref = True
                else:
                    deref = False
                if intra_reg.match(line):
                    intra = True
                else:
                    intra = False

                got_ptr = 0
                ptrinfo = []
                ptrfunc = []
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

                        if (args.count_interp_ptr):
                            ptrfunc_ret = ptrinfo_func_reg.match(newline)
                            if (ptrfunc_ret):
                                func = ptrfunc_ret.group(2)
                                vname = ptrfunc_ret.group(3)
                                vsrc = ptrfunc_ret.group(4)
                                ptrfunc += [func]
                            else:
                                # no info? assume global
                                ptrfunc += [GLOBAL_ID]
                        if (got_ptr == 2):
                            break

#                print "Missing alias:", ptrinfo
                fsptrinfo = frozenset(ptrinfo)
                missing_db.add(fsptrinfo)
                missing_detail_db[fsptrinfo] = [intra, deref]
                if (args.count_interp_ptr):
                    both_global = False
                    one_global = False
                    if (ptrfunc[0] == GLOBAL_ID):
                        if (ptrfunc[1] == GLOBAL_ID):
                            both_global = True
                        else:
                            one_global = True
                    else:
                        if (ptrfunc[1] == GLOBAL_ID):
                            one_global = True
                    if (both_global):
                        both_global_count += 1
                    elif (one_global):
                        one_global_count += 1
                    else:
                        if (ptrfunc[0] == ptrfunc[1]):
                            same_func_count += 1
                        else:
                            diff_func_count += 1

        for pair in missing_db:
            print "Missing alias:"
            detail = missing_detail_db[pair]
            vpair = list(pair)
            if (len(pair) != 2):
                vpair += [vpair[0]]
            if (vpair[0] < vpair[1]):
                u = vpair[0]
                vpair[0] = vpair[1]
                vpair[1] = u

            print "[%d]%s" % (vpair[0], value_desc_db[vpair[0]])
            print "[%d]%s" % (vpair[1], value_desc_db[vpair[1]])
            if detail[0]:
                # intra
                if detail[1]:
                    # deref
                    intra_deref+=1
                else:
                    intra_nonderef+=1
            else:
                if detail[1]:
                    inter_deref+=1
                else:
                    inter_nonderef+=1
        print "Total missing alias pairs: %d" % (len(missing_db))
        if (args.count_interp_ptr):
            print "Global pairs: %d" % both_global_count
            print "Global and local pairs: %d" % one_global_count
            print "Intra-procedure pairs: %d" % same_func_count
            print "Inter-procedure pairs: %d" % diff_func_count
        print "Intra-procedure deref pairs: %d" % intra_deref
        print "Intra-procedure non-deref pairs: %d" % intra_nonderef
        print "Inter-procedure deref pairs: %d" % inter_deref
        print "Inter-procedure non-deref pairs: %d" % inter_nonderef
        print "Total intra-procedure pairs: %d" % (intra_deref + intra_nonderef)
        print "Total inter-procedure pairs: %d" % (inter_deref + inter_nonderef)
        print "Total deref pairs: %d" % (inter_deref + intra_deref)
        print "Total non-deref pairs: %d" % (inter_nonderef + intra_nonderef)


