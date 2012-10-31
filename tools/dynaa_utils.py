import os
import sys
import string
import rcs_utils

def load_all_plugins(cmd):
    cmd = rcs_utils.load_all_plugins(cmd)
    cmd = rcs_utils.load_plugin(cmd, 'DynAAAnalyses')
    cmd = rcs_utils.load_plugin(cmd, 'DynAACheckers')
    cmd = rcs_utils.load_plugin(cmd, 'DynAAInstrumenters')
    return cmd

def get_linking_flags(prog):
    linking_flags = ['-pthread']
    if prog.startswith('pbzip2'):
        linking_flags.extend(['-lbz2'])
    if prog.startswith('ferret'):
        linking_flags.extend(['-lgsl', '-lblas'])
    if prog.startswith('gpasswd'):
        linking_flags.extend(['-lcrypt'])
    if prog.startswith('cvs'):
        linking_flags.extend(['-lcrypt'])
    if prog.startswith('httpd'):
        linking_flags.extend(['-lcrypt'])
    if prog.startswith('mysqld'):
        linking_flags.extend(['-lcrypt', '-ldl', '-lz'])
    if prog.startswith('wget'):
        linking_flags.extend(['-lrt', '-lgnutls', '-lidn'])
    return linking_flags

def load_aa(cmd, *aas):
    for aa in aas:
        # Some AAs require additional plugins.
        if aa == 'ds-aa':
            cmd = rcs_utils.load_plugin(cmd, 'LLVMDataStructure')
        elif aa == 'anders-aa' or aa == 'su-aa':
            cmd = rcs_utils.load_plugin(cmd, 'RCSAndersens')
        elif aa == 'bc2bdd-aa':
            if not os.path.exists('bc2bdd.conf'):
                sys.stderr.write('\033[1;31m')
                print >> sys.stderr, 'Error: bc2bdd-aa requires bc2bdd.conf,',
                print >> sys.stderr, 'which cannot be found in the current',
                print >> sys.stderr, 'directory.'
                sys.stderr.write('\033[m')
                sys.exit(1)
            cmd = rcs_utils.load_plugin(cmd, 'bc2bdd')
        cmd = string.join((cmd, '-' + aa))
    return cmd

def supports_intra_proc_queries_only(aa):
    return aa == 'basicaa' or aa == 'ds-aa'

def get_aa_choices():
    return ['tbaa', 'basicaa', 'no-aa', 'ds-aa', 'anders-aa', 'bc2bdd-aa',
            'su-aa']
