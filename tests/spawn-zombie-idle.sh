#!/bin/bash
#
# https://github.com/clarkwang/sexpect/issues/16

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect sp -zombie-ttl 5 sleep 5
assert_run sexpect ex -eof
for i in {1..10}; do
    run sleep 1
    assert_run sexpect ex -eof
done

run sleep 6
negass_run sexpect get
