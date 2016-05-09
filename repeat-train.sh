#!/bin/bash

i=$1

while [[ 1 = 1 ]]; do
    q=''
    [[ $[ $RANDOM % 2 ] -eq 0 ]] && q='-q phi'
    pid=`qsub ${q} latedays-train${i}.sh`
    while qstat $pid; do
        sleep 10s
    done
done

