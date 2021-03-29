#!/bin/bash
#
# https://github.com/clarkwang/sexpect/issues/18

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect sp -zombie-ttl 10 sleep 10
pid=$( sexpect get -pid )
run sleep 11

# zombie should still be walking around
assert_run kill -0 $pid

for i in {1..11}; do
    run sleep 1
    assert_run sexpect ex -eof
done

run sleep 11
negass_run sexpect get
