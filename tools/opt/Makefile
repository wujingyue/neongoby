LEVEL = ../..

TOOLNAME = dynaa_opt

LINK_COMPONENTS = bitreader bitwriter asmparser instrumentation scalaropts ipo vectorize

USEDLIBS = DynAAUtils.a DynAAAnalyses.a DynAACheckers.a DynAAInstrumenters.a DynAATransforms.a

AdditionalLibPaths = $(RCS_OBJ_ROOT)/$(BuildMode)/lib
LDFLAGS += -lRCSID -lRCSCFG -lRCSPointerAnalysis

include $(LEVEL)/Makefile.common
