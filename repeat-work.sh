#!/bin/bash

while [[ 1 = 1 ]]; do
    pid=`qsub -q phi latedays-worker.sh`
    while qstat $pid; do
        sleep 10s
    done
done

