#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

delta=10
nexit=7
assert_run sexpect sp -t 10 -cloexit -ttl 20 ./still_open_after_exit 1 $delta $nexit

assert_run sexpect ex -t 1 'child sleeping'
assert_run sexpect ex -t 2 'parent exiting'

assert_run sexpect ex -t 1 -eof

run sexpect w
ret=$?
assert "[[ $ret == $nexit ]]"
