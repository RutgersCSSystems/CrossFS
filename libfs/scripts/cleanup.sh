#! /bin/bash

set -x

cd $LIBFS
$LIBFS/benchmark/crfs_exit
sudo pkill -9 redis-benchmark
sudo pkill -9 redis-server
sudo pkill -9 db_bench
rm -f $AUXILLARY/*
set +x
