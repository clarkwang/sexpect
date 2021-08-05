#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

assert '[[ -t 0 ]]'

export PS1='\s-\v\$ '
assert_run sexpect sp -t 2 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

if (( 1 )); then
    # interact -re
    assert_run sexpect s -cr 'echo {a..z} | sed "s/ //g" '
    assert_run sexpect i -re '[abc][b][c]'
    for c in {d..z}; do
        assert_run sexpect i -re "^[$c]"
        out=$( sexpect out )
        assert '[[ $out == $c ]]' 
    done
    assert_run sexpect i -re "$re_ps1"
fi

if (( 1 )); then
    # NULs removed for pattern matching
    assert_run sexpect s -cr 'printf "foo\0\0\0\0bar\n" '
    assert_run sexpect i -re foobar
    assert_run sexpect i -re "$re_ps1"

    # NULs always sent to client
    assert_run sexpect s -cr 'printf "foo\0\0\0\0bar\n" '
    assert 'sexpect i -re foobar | tr "\0" x | grep xxxx'
    assert_run sexpect i -re "$re_ps1"
fi

assert_run sexpect s -c 'exit 0\r'
assert_run sexpect w
