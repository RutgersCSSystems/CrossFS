#! /bin/bash

BASE=$PWD/strata

set -x

cd $BASE/libfs/lib
rm -rf nvml
rm -rf syscall_intercept
rm -rf jemalloc-4.5.0

cd $BASE/libfs
make clean
cd tests
make clean

cd $BASE/kernfs
make clean
cd tests
make clean

set +x
