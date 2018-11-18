#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

#----------------------------------------------------------------------------#

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc

# `expect -eof' returns TIMEOUT if not EOF
run sexpect ex -t 1 -eof
ret=$?
assert_run sexpect ck -err $ret -is timeout

# If `expect -eof' times out, it should not clear expbuf
assert_run sexpect ex b
assert_run sexpect ex a
assert_run sexpect ex s
assert_run sexpect ex h

assert_run sexpect s -c 'exit 127\r'
# This should not fail with EOF error.
assert_run sexpect ex -ex exit
assert_run sexpect ex -t 1 -eof
assert_run sexpect ex -t 0 -eof
assert_run sexpect ex -t 0 -eof

# After EOF, even `expect -re .*' would fail.
run sexpect ex -t 0 -re '.*'
ret=$?
assert_run sexpect ck -err $ret -is eof

run sexpect w
ret=$?
assert "[[ $ret == 127 ]]"

#----------------------------------------------------------------------------#

run sleep 1
assert_run sexpect sp -t 10 -ttl 20 bash --norc
assert_run sexpect s -c 'exit\r'
run sleep 1
# `expect' w/o a pattern defaults to `-re ".*" '
assert_run sexpect ex
run sexpect ex -t 1 not-found
ret=$?
assert_run sexpect ck -err $ret -is eof
assert_run sexpect w
