#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

tmpfile=/tmp/file-with-nulls

assert_run sexpect s -cr "cat > $tmpfile"
assert_run sexpect s -cr -c 'a\000\000b'
assert_run sexpect s -c '\cd'
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -cr "od -Ax -tx1 -v -N 128 $tmpfile | sed 's/  */ /g' "
assert_run sexpect ex '61 00 00 62'
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -cr exit
assert_run sexpect w
