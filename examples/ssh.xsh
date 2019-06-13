#!/bin/bash
#
# Usage:
#   ./ssh.xsh -p PASSWORD [USER@]HOST [COMMAND ...]
#

function fatal()
{
    echo "!! $*"
    exit 1
}

if [[ $# -lt 3 || $1 != -p ]]; then
    fatal "Usage: $0 -p PASSWORD [USER@]HOST [COMMAND ...]"
fi

passwd=$2; shift 2

export SEXPECT_SOCKFILE=/tmp/sexpect-ssh-$$.sock

sexpect spawn -t 10 \
    ssh -o PreferredAuthentications=keyboard-interactive,password \
        -o NumberOfPasswordPrompts=1 \
        -o ControlPath=none \
        "$@"

while true; do
    sexpect expect -nocase -re 'password:|yes/no'
    ret=$?
    if [[ $ret == 0 ]]; then
        out=$( sexpect expect_out )
        if [[ $out == yes/no ]]; then
            sexpect send -enter yes
            continue
        else
            sexpect send -enter "$passwd"
            break
        fi
    elif sexpect chkerr -errno $ret -is eof; then
        sexpect wait
        exit
    elif sexpect chkerr -errno $ret -is timeout; then
        sexpect close
        sexpect wait
        fatal "timeout waiting for password prompt"
    else
        fatal "unknown error: $ret"
    fi
done

sexpect set -idle 1
sexpect interact
