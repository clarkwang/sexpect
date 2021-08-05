#!/bin/bash
#
# The daemon chdir'ing to `/' should not impact the spawned process' current
# dir, otherwise the command
#
#   % sexpect sp vim somefile
#
# would actually become
#
#   % sexpect sp vim /somefile
#

source $SRCDIR/tests/common.sh || exit 1

tmpfile=chdir-after-exec.tmp.xYcyTm
assert_run rm -f $tmpfile
negass_run ls -l $tmpfile
assert_run touch $tmpfile

#
# For bash-5.1.4 there seems to be some race condition here. Without trapping
# SIGHUP bash will be killed with SIGHUP and `sexpect w' would fail.
#
assert_run sexpect sp -t 2 -ttl 20 bash -c "trap '' HUP; pwd; sleep 1; ls -l *.xYcyTm; rm -f *.xYcyTm"
assert_run sexpect ex $tmpfile
assert_run sexpect ex -eof
assert_run sexpect ex -eof
assert_run sexpect w
