#! /bin/bash

set -x

PARAFS=$PWD

#export LD_LIBRARY_PATH=$PARAFS/libshim/glibc-build/rt/:/usr/local/lib:/usr/lib/x86_64-linux-gnu/:/lib/x86_64-linux-gnu/

LD_PRELOAD=$PARAFS/libshim/shim.so $PARAFS/benchmark/test_shim

set +x
