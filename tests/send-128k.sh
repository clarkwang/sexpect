#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

x1k=$( str_repeat x 1024 )
x128k=$( str_repeat $x1k 128 )
md5=3832e28c8feea48397f30d70b43d7987

assert_run sexpect s -cr "printf '%s' $x128k | md5sum"
assert_run sexpect ex -i $md5
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -cr exit
assert_run sexpect w
