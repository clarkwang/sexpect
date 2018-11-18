#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

#----------------------------------------------------------------------------#

for arg in '' -TERM -SIGTERM -term -sigterm -15; do
    assert_run sexpect sp -t 10 -ttl 20 bash -c 'echo hello; sleep 60'
    assert_run sexpect ex hello
    assert_run sexpect k $arg
    assert_run sexpect ex -t 1 -eof
    run sexpect w
    ret=$?
    assert "[[ $ret == 143 ]]"

    sleep .2
    for ((i = 0; i < 10; ++i)); do
        if [[ ! -f $SEXPECT_SOCKFILE ]]; then
            break
        fi
        sleep .1
    done
done
