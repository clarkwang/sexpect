#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

delta=5
nexit=7
assert_run sexpect sp -ttl 20 ./still_open_after_exit 1 $delta $nexit

assert_run sexpect ex -t 1 'child sleeping'
assert_run sexpect ex -t 2 'parent exiting'
    # pty still being opened by the grandchild
run sexpect ex -t 1 -eof
ret=$?
assert_run sexpect ck -err $ret -is timeout

assert_run sexpect ex -t $(( delta + 1 )) 'child exiting'
assert_run sexpect ex -t 1 -eof

run sexpect w
ret=$?
assert "[[ $ret == $nexit ]]"
