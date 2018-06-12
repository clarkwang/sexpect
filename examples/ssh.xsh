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
trap '{ sexpect c && sexpect w; } >& /dev/null' EXIT

sexpect spawn \
    ssh -o PreferredAuthentications=keyboard-interactive,password \
        -o NumberOfPasswordPrompts=1 \
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

sexpect interact
