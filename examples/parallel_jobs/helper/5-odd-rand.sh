#!/bin/bash
#
# Exit when we've found 5 continuous odd random integers (in range [0, 65535]).
#

randint()
{
    local arr=( $( od -N 2 -d /dev/urandom ) )
    echo ${arr[1]}
}

max=5
interval=0.5
n=0
for ((i=1; ; ++i)); do
    r=$( randint )
    printf '[%3d] %5d\n' $i $r
    if (( r % 2 == 1 )); then
        if (( ++n == max )); then
            echo "Got $max continuous odd numbers in $SECONDS seconds."
            break
        fi
    else
        n=0
    fi

    sleep $interval
done
