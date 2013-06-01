#!/bin/sh

die () {
	echo "$@" 1>&2
	exit 1
}

test -f configure.ac || die "Can't find 'autoconf' dir; please cd into it first"

autoconf --version | egrep '2\.[56][0-9]' > /dev/null
if test $? -ne 0 ; then
  die "Your autoconf was not detected as being 2.5x or 2.6x"
fi

echo "Regenerating aclocal.m4 with aclocal"
rm -f aclocal.m4
aclocal -I `llvm-config --src-root`/autoconf/m4 || die "aclocal failed"

echo "Regenerating configure with autoconf"
autoconf --warnings=all -o configure configure.ac || die "autoconf failed"

exit 0
