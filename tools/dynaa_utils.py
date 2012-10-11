import os
import string
import rcs_utils

def load_all_plugins(cmd):
    cmd = rcs_utils.load_all_plugins(cmd)
    cmd = rcs_utils.load_plugin(cmd, 'DynAAAnalyses')
    cmd = rcs_utils.load_plugin(cmd, 'DynAACheckers')
    cmd = rcs_utils.load_plugin(cmd, 'DynAAInstrumenters')
    return cmd
