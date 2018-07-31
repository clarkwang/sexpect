#!/bin/bash
#
#  read(ptm) returning EOF does not necessarily mean the child has exited.
#

source $SRCDIR/tests/common.sh || exit 1

nsleep=5
nexit=7
assert_run sexpect sp -ttl 30 ./eof_before_exit $nsleep $nexit
    # We get EOF but the child's still running.
assert_run sexpect ex -t 2 -eof
assert_run sexpect ex -t 0 -eof
run sexpect ex -t 0 -re '.*'
ret=$?
assert_run sexpect ck -err $ret -is eof

t1=$SECONDS
run sexpect w
ret=$?
t2=$SECONDS
assert "(( $t2 - $t1 > $nsleep - 2 ))"
assert "[[ $ret == $nexit ]]"
