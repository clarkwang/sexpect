#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

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
