#!/bin/bash

set -x

#LD_PRELOAD=./io-analyser/shim.so ./filebench -f myworkloads/filemicro_rwrite.f
LD_PRELOAD=./io-analyser/shim.so ./filebench -f myworkloads/filemicro_rread.f

set +x
