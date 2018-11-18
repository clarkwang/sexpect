#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

re_ps1='bash-[.0-9]+[$#] $'
assert_run sexpect ex -re "$re_ps1"

# expect -re
assert_run sexpect s -cr 'printf %d {1..9}; echo'
assert_run sexpect ex -re '(1)(2)(3)(4)(5)(6)(7)(8)(9)'

out=$( sexpect out )
assert_run test 123456789 = "$out"
out=$( sexpect out -i 0 )
assert_run test 123456789 = "$out"

for i in {1..9}; do
    out=$( sexpect out -i $i )
    assert_run test $i = "$out"
done

assert_run sexpect ex -re "$re_ps1"

# expect -ex
assert_run sexpect s -cr 'printf %d {9..1}; echo'
assert_run sexpect ex 987654321

out=$( sexpect out )
assert_run test 987654321 = "$out"
out=$( sexpect out -i 0 )
assert_run test 987654321 = "$out"

for i in {1..9}; do
    out=$( sexpect out -i $i )
    assert_run test x = "x$out"
done

assert_run sexpect ex -re "$re_ps1"

assert_run sexpect s -c 'exit 0\r'
assert_run sexpect w
