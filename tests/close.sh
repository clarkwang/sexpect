#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

#----------------------------------------------------------------------------#

assert_run sexpect sp sleep 60
assert_run sexpect c
assert_run sexpect c
assert_run sexpect c
assert_run sexpect c
assert_run sexpect ex -t 1 -eof
run sexpect w
ret=$?
assert "[[ $ret == 129 ]]"
