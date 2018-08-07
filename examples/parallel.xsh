#!/bin/bash
#
# Start multiple jobs in parallel and monitor each job's output until all done.
#

njobs=3
socks=()

script=helper/5-odd-rand.sh
if [[ ! -f $script ]]; then
    echo "ERROR: \`$script' not found"
    exit 1
fi

#
# start jobs
#
for ((i = 0; i < njobs; ++i)); do
    sock=/tmp/sexpect-parallel-example.sock.$i
    socks[i]=$sock

    sexpect -s $sock sp bash $script
done

#
# monitor jobs
#
interval=3
ndone=0
jobstats=()
while true; do
    if (( ndone == njobs )); then
        break
    fi

    for ((i = 0; i < njobs; ++i)); do
        if (( jobstats[i] )); then
            continue
        fi

        sock=${socks[i]}

        echo
        echo "==========> job $((i+1)) <=========="
        if (( ndone == njobs - 1 )); then
            sexpect -s $sock ex -lb 5 -t -1 -eof
        else
            sexpect -s $sock ex -lb 5 -t $interval -eof
        fi
        ret=$?
        if [[ $ret == 0 ]]; then
            (( ++ndone ))
            jobstats[i]=1
        elif sexpect chk -err $ret -is timeout; then
            continue
        fi
    done
done

#
# job results
#
echo
echo '------------------------ FINAL RESULT ------------------------'
for ((i = 0; i < njobs; ++i)); do
    sock=${socks[i]}
    echo
    echo "==== job $((i+1)) ===="
    sexpect -s $sock i -lb 6
done
