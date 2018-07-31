#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

#----------------------------------------------------------------------------#

assert_run sexpect sp -ttl 20 bash -c 'while true; do sleep 1; xxd -l 8192 /dev/urandom; done'

for ((i = 0; i < 10; ++i)); do
    run sexpect ex -t 1 -eof
    ret=$?
    assert_run sexpect ck -err $ret -is timeout
done

assert_run sexpect c
negass_run sexpect w
