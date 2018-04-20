#!/bin/bash

if ! which sexpect >& /dev/null; then
    echo "sexpect not found in your \$PATH"
    exit 1
fi

tmpsock=$(mktemp /tmp/sexpect-tmp-XXXXXX.sock)
if [[ ! -f $tmpsock ]]; then
    echo "failed to create tmp sock file"
    exit 1
else
    rm -f $tmpsock
    export SEXPECT_SOCKFILE=$tmpsock
fi

sexpect sp -t 2 -nowait env -i bc

if ! sexpect ex warranty; then
    sexpect k -kill

    echo "oops! aren't you using GNU bc?"
    exit 1
fi

sum=0
max=100
for ((i = 1; ; ++i)); do
    sexpect s -cr "$sum+$i"
    if ! sexpect ex -re '[[:space:]]([0-9]+)[[:space:]]'; then
        sexpect k -kill

        echo "oops! something wrong"
        exit 1
    fi
    sum=$(sexpect out -i 1)

    if [[ $sum -gt $max ]]; then
        sexpect s -c '\cd'
        exit
    fi
done
