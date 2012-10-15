import os
import string
import rcs_utils

def load_all_plugins(cmd):
    cmd = rcs_utils.load_all_plugins(cmd)
    cmd = rcs_utils.load_plugin(cmd, 'DynAAAnalyses')
    cmd = rcs_utils.load_plugin(cmd, 'DynAACheckers')
    cmd = rcs_utils.load_plugin(cmd, 'DynAAInstrumenters')
    return cmd

def get_extra_linking_flags(prog):
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
    return linking_flags
