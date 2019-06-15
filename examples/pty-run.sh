#!/bin/bash

type -P sexpect >& /dev/null || exit 1

sock=/tmp/pty-sexpect-$$.sock
sexpect -s $sock spawn -idle 10 "$@"
sexpect -s $sock interact
