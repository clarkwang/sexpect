#!/bin/bash
#
# `ps -p <pid> -o stat' also works on macOS.
#

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect sp -t 10 -nonblock -ttl 20 bash -c 'echo aaa; sleep 5; echo bbb; sleep 2'
run sleep 1

# `aaa' can still be expect'ed.
assert_run sexpect ex -t 2 aaa

# `bbb' shows up when we're actively waiting, so make us happy.
assert_run sexpect ex -t 5 bbb

assert_run sexpect ex -t 10 -eof
assert_run sexpect ex -t 0 -eof
assert_run sexpect w

#----------------------------------------------------------------------------#

assert_run sexpect sp -ttl 5 od -v /dev/zero
run sleep 1
pid=$( sexpect get -pid )
info "pid=$pid"
st=$( ps -p $pid -o stat | sed -n 2p )
info "st=$st"
assert '[[ $st == S* ]]'

assert_run sexpect set -nonblock 1
run sleep .5
st=$( ps -p $pid -o stat | sed -n 2p )
info "st=$st"
assert '[[ $st == R* ]]'

assert_run sexpect set -nonblock 0
run sleep 1
st=$( ps -p $pid -o stat | sed -n 2p )
info "st=$st"
assert '[[ $st == S* ]]'

assert_run sexpect set -nowait 1
assert_run sexpect c

#----------------------------------------------------------------------------#

assert_run sexpect sp -nonblock -ttl 5 od -v /dev/zero
run sleep 1
pid=$( sexpect get -pid )
info "pid=$pid"
#
# There's some race condition here. The pty/master side may be not reading
# od's output quickly enough so at some time od may be in sleeping state.
#
for i in {1..100}; do
    st=$( ps -p $pid -o stat | sed -n 2p )
    info "st=$st"
    if [[ $st == R* ]]; then
        break
    fi
done

assert_run sexpect set -nonblock 0
run sleep 1
st=$( ps -p $pid -o stat | sed -n 2p )
info "st=$st"
assert '[[ $st == S* ]]'

assert_run sexpect set -nonblock 1
run sleep 1
st=$( ps -p $pid -o stat | sed -n 2p )
info "st=$st"
assert '[[ $st == R* ]]'

assert_run sexpect set -nowait 1
assert_run sexpect c
