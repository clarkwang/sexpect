#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect sp -t 10 -ttl 3 sleep 300
run sleep 1
assert_run sexpect get

run sleep 5
negass_run sexpect get
