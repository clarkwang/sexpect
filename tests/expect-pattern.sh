#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

if (( 1 )); then
    # expect -ex
    assert_run sexpect s -cr 'echo {a..z} | sed "s/ //g" '
    assert_run sexpect ex abc
    for c in {d..z}; do
        assert_run sexpect ex $c
    done
    assert_run sexpect ex -re "$re_ps1"

    # expect -glob
    assert_run sexpect s -cr 'echo {a..z} | sed "s/ //g" '
    assert_run sexpect ex -gl '[a][b][c]'
    for c in {d..z}; do
        assert_run sexpect ex -gl "[$c]"
    done
    assert_run sexpect ex -re "$re_ps1"

    # expect -re
    assert_run sexpect s -cr 'echo {a..z} | sed "s/ //g" '
    assert_run sexpect ex -re '[abc][b][c]'
    for c in {d..z}; do
        assert_run sexpect ex -re "[$c]"
    done
    assert_run sexpect ex -re "$re_ps1"
fi

# NULs removed for pattern matching
assert_run sexpect s -cr 'printf "foo\0\0\0\0bar\n" '
assert_run sexpect ex foobar
assert_run sexpect ex -re "$re_ps1"

# NULs always sent to client
assert_run sexpect s -cr 'printf "foo\0\0\0\0bar\n" '
assert 'sexpect ex foobar | tr "\0" x | grep xxxx'
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -c 'exit 0\r'
assert_run sexpect w
