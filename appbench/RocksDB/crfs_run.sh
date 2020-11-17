#! /bin/bash
set -x

RESULT_BASE=$BASE/results/apps-$DEVICE/parafs-noioctl
result_dir=$RESULT_BASE/rocksdb
mkdir -p $result_dir
#RESULTDIR=result-parafs-$1
# Run fillseq, readseq banchmark
#./parafs_run_seq.sh $RESULTDIR
sleep 2

# Run fillrandom, readrandom banchmark
./parafs_run_rand.sh $result_dir


