#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -c 'echo FOO\r'
assert_run sexpect ex -t 1 -i foo
assert_run sexpect ex -t 1 -i foo
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -c 'echo BAR\r'
assert_run sexpect ex -t 1 -i -re bar
assert_run sexpect ex -t 1 -i -re bar
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -enter exit
assert_run sexpect w
