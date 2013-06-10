import os
import sys
import string
import rcs_utils

def load_all_plugins(cmd):
    cmd = rcs_utils.load_all_plugins(cmd)
    cmd = rcs_utils.load_plugin(cmd, 'libDynAAUtils')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAAAnalyses')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAACheckers')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAAInstrumenters')
    cmd = rcs_utils.load_plugin(cmd, 'libDynAATransforms')
    return cmd

def load_aa(cmd, *aas):
    for aa in aas:
        # Some AAs require additional plugins.
        if aa == 'ds-aa':
            cmd = rcs_utils.load_plugin(cmd, 'LLVMDataStructure')
        elif aa == 'anders-aa' or aa == 'su-aa':
            cmd = rcs_utils.load_plugin(cmd, 'RCSAndersens')
        elif aa == 'bc2bdd-aa':
            if not os.path.exists('bc2bdd.conf'):
                sys.stderr.write('\033[0;31m')
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
            'su-aa', 'scev-aa']
