# Author: Jingyue

PROGS = SimpleTest TestCalloc TestStore TestInit TestThread TestTopLevel
INSTRUMENTED_EXES = $(PROGS:=.inst)
DOTS = $(PROGS:=.dot)
PTSS = $(PROGS:=.pts)
PSS = $(PROGS:=.ps)
PNGS = $(PROGS:=.png)

all: $(PSS) $(PNGS)

clean:
	rm -rf $(PTSS) $(DOTS) $(PSS) $(PNGS)

%.ps: %.dot
	dot $< -o $@ -Tps

%.png: %.dot
	dot $< -o $@ -Tpng

%.dot: %.pts
	display-pts < $< > $@

%.pts: ../Instrumented/%.inst
	$<
	mv /tmp/pts $@

SimpleTest.pts: ../Instrumented/SimpleTest.inst
	$< 1024
	mv /tmp/pts $@

TestCalloc.pts: ../Instrumented/TestCalloc.inst
	$< 10 1024
	mv /tmp/pts $@

.PHONY: clean
