#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

#
# without `-nohup'
#
assert_run sexpect sp -t 10 -ttl 20 sleep 300
run sleep 1
assert_run sexpect c
assert_run sexpect ex -t 1 -eof
run sexpect w
ret=$?
assert "[[ $ret == 129 ]]"

# wait a little while for the last sexpect server to exit
run sleep 1

#
# with `-nohup'
#
assert_run sexpect sp -t 10 -ttl 20 -nohup sleep 300
run sleep 1
assert_run sexpect c
# pty closed so we'd get EOF
assert_run sexpect ex -t 1 -eof

# SIGTERM
assert_run sexpect k -15
run sexpect w
ret=$?
assert "[[ $ret == 143 ]]"
