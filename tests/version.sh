#!/bin/bash

source $SRCDIR/tests/common.sh || exit 1

assert_run sexpect version
assert_run sexpect -version
assert_run sexpect --version
