#!/bin/bash
#
# In bc, calculate 1+2+3+... until the sum > 100.
#

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
    sexpect s -cr "$sum + $i"
    if ! sexpect ex -cstr -re '[\r\n]([0-9]+)[\r\n]'; then
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

#
# EXAMPLE OUTPUT:
#
#   $ bash bc.sexp.sh
#   bc 1.06.95
#   Copyright 1991-1994, 1997, 1998, 2000, 2004, 2006 Free Software Foundation, Inc.
#   This is free software with ABSOLUTELY NO WARRANTY.
#   For details type `warranty'.
#   0 + 1
#   1
#   1 + 2
#   3
#   3 + 3
#   6
#   6 + 4
#   10
#   10 + 5
#   15
#   15 + 6
#   21
#   21 + 7
#   28
#   28 + 8
#   36
#   36 + 9
#   45
#   45 + 10
#   55
#   55 + 11
#   66
#   66 + 12
#   78
#   78 + 13
#   91
#   91 + 14
#   105
#
