#!/bin/bash
TRACE=$2
JOBNAME=$PBS_JOBID

cd $PBS_O_WORKDIR
PATH=$PATH:$PBS_O_PATH

USERNAME=`whoami`
MASTER=`head -n1 $PBS_NODEFILE`
NODES=`sort -u $PBS_NODEFILE`

echo $LD_LIBRARY_PATH
echo "spawning master process on $HOSTNAME ($PBS_NODENUM of $PBS_NUM_NODES)"
echo $NODES
echo "Username is " $USERNAME

echo "command was: '$CMD $@'"


dir=/home/jpdoyle/OmegaGo

cd ${dir}
./training.sh 1024 ${dir}/nn4.txt 4 4 $((2 * 46)) $SCRATCH


