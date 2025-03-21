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

sexpect send -enter 'sum=0'
sexpect expect -re "$ps1re"

for ((i = 1; i <= 200; ++i)); do
    sexpect send -enter "(( sum += $i )); echo sum=\$sum"
    if ! sexpect expect -t 5 -cstr -re "sum=([0-9]+).*$ps1re"; then
        fatal "failed to wait for bash prompt or the final 5050"
    fi
    out=$( sexpect expect_out -index 1 )
    if [[ $out == '5050' ]]; then
        break
    fi
done

if [[ $out == '5050' ]]; then
    sexpect send -enter ": cool we finally got 5050"
else
    sexpect send -enter ": oops we did not get 5050"
fi
sexpect expect -re "$ps1re"

sexpect send -enter exit
sexpect wait
