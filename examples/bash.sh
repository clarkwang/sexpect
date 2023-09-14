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

ps1re='bash-[0-9.]+[$#] $'
sexpect spawn -idle 10 bash --norc
if ! sexpect expect -t 5 -re "$ps1re"; then
    fatal "timed out waiting for the shell prompt"
fi

commands=(
    'echo hello world'
    'date'
    'echo "oN wnii tnrecaitevm do e..." | sed '\'s/'\(.\)\(.\)/\2\1/g'\'
)
for cmd in "${commands[@]}"; do
    sexpect send -cr "$cmd"
    sexpect expect -re "$ps1re"
done

sexpect interact -c -sub "([\r\n]+)?($ps1re)::(1)\e[1;35m(2)\e[m"
