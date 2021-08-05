#!/bin/bash
#
# The daemon should chdir("/") after open("logfile"), otherwise the command
#
#   % sexpect sp -logfile foo.log command...
#
# would actually become
#
#   % sexpect sp -logfile /foo.log command...
#

source $SRCDIR/tests/common.sh || exit 1

tmpfile=chdir-after-logfile.vJv0TT.log
assert_run rm -f $tmpfile
negass_run ls -l $tmpfile

assert_run sexpect sp -t 10 -logf $tmpfile -ttl 30 ls -l /
assert_run sexpect ex -eof
assert_run sexpect w
assert_run ls -l $tmpfile
run rm -f $tmpfile
