#!/bin/sh
die () {
	echo "$@" 1>&2
	exit 1
}
test -d autoconf && test -f autoconf/configure.ac && cd autoconf
test -f configure.ac || die "Can't find 'autoconf' dir; please cd into it first"
autoconf --version | egrep '2\.[56][0-9]' > /dev/null
if test $? -ne 0 ; then
  die "Your autoconf was not detected as being 2.5x or 2.6x"
fi
while true ; do
	read -p "Enter full path to LLVM source:" REPLY
	if test -d "$REPLY/autoconf/m4" ; then
		llvm_m4="$REPLY/autoconf/m4"
		break
	fi
	echo "LLVM source root not found."
done
while true ; do
	read -p "Enter full path to RCS common source:" REPLY
	if test -d "$REPLY/autoconf/m4" ; then
		rcs_common_m4="$REPLY/autoconf/m4"
		break
	fi
	echo "RCS common source root not found."
done
echo "Regenerating aclocal.m4 with aclocal"
rm -f aclocal.m4
aclocal -I $rcs_common_m4 -I $llvm_m4 || die "aclocal failed"
echo "Regenerating configure with autoconf"
autoconf --warnings=all -o ../configure configure.ac || die "autoconf failed"
cd ..
exit 0
