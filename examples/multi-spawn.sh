#!/usr/bin/env bash

bashexp() { sexpect -s /tmp/sexpect-bash-$$.sock "$@"; }
pythexp() { sexpect -s /tmp/sexpect-pyth-$$.sock "$@"; }

ps1bash='bash-[0-9.]+[$#] $'
ps1pyth='>>> $'

bashexp spawn bash --norc
pythexp spawn python3

bashexp expect -re "$ps1bash"; echo
pythexp expect -re "$ps1pyth"; echo

sum=0
for ((i = 1; i <= 10; ++i)); do
    if ((i % 2)); then  # bash to add odd numbers
        bashexp expect -lookback 1
        bashexp send -enter "echo sum=\$(($sum + $i))"
        bashexp expect -re 'sum=([0-9]+)'
        sum=$( bashexp expect_out -i 1 )

        bashexp expect -re "$ps1bash"; echo
    else  # python to add even numbers
        pythexp expect -lookback 1
        pythexp send -enter "print('sum=%d' % ($sum + $i))"
        pythexp expect -re 'sum=([0-9]+)'
        sum=$( pythexp out -i 1 )

        pythexp expect -re "$ps1pyth"; echo
    fi
done

bashexp expect -lookback 1
bashexp send -enter 'exit # Bash exiting ...'
bashexp wait

pythexp expect -lookback 1
pythexp send -enter 'exit() # Python exiting ...'
pythexp wait

echo "---- Final sum: $sum ----"
