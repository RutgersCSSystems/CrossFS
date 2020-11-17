#!/bin/bash

set -x

# Set glolbal variables
source scripts/setvars.sh

# Compile libfs code
cd devfs_client
make clean && make && make install

# Mount ParaFS
source scripts/setvars.sh
scripts/mountdevfs_sk.sh

# Run microbench
scripts/run_scalability_parafs.sh
scripts/run_scheduler_NwriterNreader.sh

# Run Redis
cd ../appbench/redis-3.0.0/
make
sudo make install
sudo cp redis.conf/etc/
sudo run_benchmark.sh

# Run Rocksdb
cd ../RocksDB/
./build_rocksdb.sh
parafs_run.sh result-parafs-new

set +x
