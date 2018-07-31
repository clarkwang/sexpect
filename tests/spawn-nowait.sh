#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

#----------------------------------------------------------------------------#

    # spawn -nowait
assert_run sexpect sp -now -ttl 20 sh
assert_run sexpect ex -t 1 -re .
assert_run sexpect s -cr exit
run sleep 1
negass_run sexpect get

    # set -nowait on
assert_run sexpect sp -ttl 20 sh
assert_run sexpect ex -t 1 -re .
assert_run sexpect s -cr exit
assert_run sexpect ex -t 1 -eof
assert_run sexpect ex -t 1 -eof
assert_run sexpect get
assert_run sexpect get
assert_run sexpect set -nowait on
negass_run sexpect get

#----------------------------------------------------------------------------#
#
# -nowait should also wait for EOF
#

delta=5
nexit=7
assert_run sexpect sp -nowait -ttl 20 ./still_open_after_exit 1 $delta $nexit

assert_run sexpect ex -t 1 'child sleeping'
assert_run sexpect ex -t 2 'parent exiting'

t1=$SECONDS
for ((i = 0; i < (delta + 1) * 5; ++i)); do
    run sleep .2
    sexpect get -pid || break
done
t2=$SECONDS
negass_run sexpect get
assert "(( $t2 - $t1 >= $delta - 2)) && (( $t2 - $t1 <= $delta + 1 ))"
