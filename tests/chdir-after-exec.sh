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
negass_run ls -l $tmpfile
assert_run touch $tmpfile

assert_run sexpect sp -t 10 -ttl 20 bash -c "sleep 1; ls -l chdir*.tmp.*; rm -f chdir*.tmp.*"
assert_run sexpect ex $tmpfile
assert_run sexpect ex -eof
assert_run sexpect w
