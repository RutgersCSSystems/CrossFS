#!/bin/bash

INSTALL_LIB() {
	sudo apt-get -y update
	sudo apt-get -y install libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev
}

COMPILE_ROCKSDB() {
	DEBUG_LEVEL=0 make shared_lib db_bench -j16
}

INSTALL_LIB
COMPILE_ROCKSDB



