NeonGoby Alias Analysis Checker
===============================

Building NeonGoby
-----------------

Build LLVM 3.0/3.1 and clang 3.0/3.1 from source code.

Build [RCS common utilities](https://github.com/wujingyue/rcs).

Finally, build Loom:

    ./configure \
        --with-rcssrc=<rcs srouce directory> \
        --with-rcsobj=<rcs object directory> \
        --prefix=`llvm-config --prefix`
    make
    make install

Running NeonGoby
----------------

**Offline mode**

As an example, say we want to check LLVM's basic alias analysis (`basicaa`) with
a test program `hello.cpp`.

1. Generate the bitcode of the test program using clang.


    clang++ hello.cpp -o hello.bc -c -emit-llvm

2. Instrument the test program. `dynaa_hook_mem.py -h` shows you more options to
   tweak the instrumentation.


    dynaa_hook_mem.py hello

3. Run the instrumented test program. NeonGoby generates the log file at
   `/tmp/pts-<pid>` by default. You can change the location by specifying
environment variable `LOG_FILE`.


    ./hello.inst

4. Check the alias analysis results against the aliases in the real execution.


    dynaa_check_aa.py hello.bc <log file> basicaa

**Online mode**

1. Generate the bitcode as in the offline mode.

2. Insert alias checks to the program. You can use option `action-if-missed` to
   specify what to do when detecting a missing alias: `report` means printing
the missing alias to `/tmp/report-<pid>` and continuing executing the test
program; `abort` means aborting the program; `silence` means doing nothing which
is for internal use.


    dynaa_insert_alias_checker.py --action-if-missed=report hello basicaa

3. Run the output executable. According to which action you specified in step 2,
   the program will abort on the first missing alias or report all missing
aliases.


    ./hello.ac

Utilities
---------

Use `dump_dump_log` to dump .pts files:

    dynaa_dump_log -log-file hello.pts > hello.log
