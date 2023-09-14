#!/usr/bin/env bash

fatal()
{
    printf '%s\n' "$*" >&2
    exit 1
}

if ! which sexpect >& /dev/null; then
    fatal 'sexpect not found in your $PATH'
fi

export SEXPECT_SOCKFILE=/tmp/sexpect-$$.sock

sexpect spawn -idle 10 python3
if ! sexpect expect -t 5 ">>> "; then
    fatal "timed out waiting for the '>>> ' prompt"
fi

commands=(
    'from pprint import pprint as pp'
    'import json'
    'import os'
    'import random'
    'import re'
    'import sys'
    'import time'
    'print(re.sub("(.)(.)", r"\2\1", "oN wnii tnrecaitevm do e...") )'
)
for cmd in "${commands[@]}"; do
    sexpect send -cr "$cmd"
    sexpect expect ">>> "
done

sexpect interact -c -sub '([\r\n]+)?(>>> )$::(1)\e[1;35m(2)\e[m'
