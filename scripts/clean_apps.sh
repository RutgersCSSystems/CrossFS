#! /bin/bash

BASE=$PWD/appbench/

set -x

cd $BASE/redis-3.0.0
sudo make clean

set +x
