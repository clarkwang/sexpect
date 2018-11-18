#!/bin/bash
#
# After the child exits (SIGCHLD) and before closing ptm, there may
# still be some data in the ptm side for reading.
#

source $SRCDIR/tests/common.sh || exit 1

export PS1='\s-\v\$ '
assert_run sexpect sp -t 10 -ttl 20 bash --norc
assert_run sexpect s -cr exit
assert_run sexpect ex -t 1 bash
assert_run sexpect ex -t 1 exit
assert_run sexpect ex -t 1 -eof
assert_run sexpect w
