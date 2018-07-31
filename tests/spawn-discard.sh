#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect sp -discard -ttl 20 bash -c 'echo aaa; sleep 5; echo bbb; sleep 2'
run sleep 1

# `aaa' is discarded so `expect' would not see it.
run sexpect ex -t 2 aaa
ret=$?
assert_run sexpect ck -err $ret -is timeout

# `bbb' shows up when we're actively waiting, so make us happy.
assert_run sexpect ex -t 5 bbb

assert_run sexpect ex -t 10 -eof
assert_run sexpect ex -t 0 -eof
assert_run sexpect w
