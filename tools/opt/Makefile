LEVEL = ../..

TOOLNAME = ng_opt

LINK_COMPONENTS = bitreader bitwriter asmparser instrumentation scalaropts ipo vectorize

# Put basic libraries at the back.
USEDLIBS = DynAATransforms.a DynAAInstrumenters.a DynAACheckers.a DynAAAnalyses.a DynAAUtils.a

include $(LEVEL)/Makefile.common

# Put basic libraries at the back.
# Append to ProjLibsOptions to make sure RCS libraries are between NeonGoby
# libraries and LLVM libraries
# FIXME: If RCS libraries changed, ng_opt will not be recompiled due to lack
# of dependencies.
ProjLibsOptions += -lRCSPointerAnalysis -lRCSCFG -lRCSID
