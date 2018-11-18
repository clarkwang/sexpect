#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect sp -t 10 -ttl 20 sleep 3
run sexpect ex -t 1 not-found
ret=$?
assert_run sexpect ck -err $ret -is timeout

run sexpect ex -t 5 not-found
ret=$?
assert_run sexpect ck -err $ret -is eof

assert_run sexpect w

for err in 0 1; do
    run sexpect ck -err $err -is eof
    ret=$?
    assert "[[ $ret == 1 ]]"

    run sexpect ck -err $err -is timeout
    ret=$?
    assert "[[ $ret == 1 ]]"
done
