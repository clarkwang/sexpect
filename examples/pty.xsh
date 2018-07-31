#!/bin/bash

type -P sexpect >& /dev/null || exit 1

sock=/tmp/pty-sexpect-8iSVDk.sock
sexpect -s $sock sp -now "$@"
sexpect -s $sock i
