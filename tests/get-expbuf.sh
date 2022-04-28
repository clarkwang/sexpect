#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -c '\cc'
assert_run sexpect s -c '\cc'
sleep .5
assert_run sexpect get -expbuf 2000
out=$( sexpect get -expbuf 2000 )
eval sexpect ex -t 0 "$out" || fatal "get -expbuf failed"
negass_run sexpect ex -t 0 -re .

assert_run sexpect s -cr
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -cr 'xxd -l 1024 /dev/urandom'
sleep .5
assert_run sexpect get -expbuf 100
out=$( sexpect get -expbuf 100 )
eval sexpect ex -t 0 "$out" || fatal "get -expbuf failed"
negass_run sexpect ex -t 0 -re .

assert_run sexpect s -cr
assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -cr 'exit 0'
assert_run sexpect w
