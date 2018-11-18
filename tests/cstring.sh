#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

# printf 'foo%sbar\n' '\\'
assert_run sexpect s  -c 'printf \47foo%sbar\\n\x27 \047\\\\\x27 \r'
assert_run sexpect ex -c 'printf \47foo%sbar\\n\x27 \047\\\\\x27 '
assert_run sexpect ex 'foo\\bar'
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -c 'exit 0\r'
assert_run sexpect w
