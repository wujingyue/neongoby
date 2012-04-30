#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = .
DIRS = lib runtime tools test

#
# Include the Master Makefile that knows how to build all.
#
include $(LEVEL)/Makefile.common
