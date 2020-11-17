#! /bin/bash

BASE=$PWD/strata

set -x

sudo apt-get install libssl-dev gawk libaio-dev libcunit1-dev autoconf dh-autoreconf asciidoctor libkmod-dev libudev-dev uuid-dev libjson-c-dev librdmacm-dev libibverbs-dev libkeyutils-dev

# Build required libs
cd $BASE/libfs/lib

# Get NVML Library
git clone https://github.com/pmem/nvml
git clone https://github.com/pmem/syscall_intercept.git
make

# Get NDCTL Library
git clone https://github.com/pmem/ndctl

cd ndctl
./autogen.sh
./configure CFLAGS='-g -O2' --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib
make
make check
sudo make install
cd ..
rm -rf ndctl

# Build jemalloc
cd $BASE/libfs/lib
tar xvjf jemalloc-4.5.0.tar.bz2
cd jemalloc-4.5.0
./autogen
./configure
make

cd $BASE/libfs
make
cd tests
make

cd $BASE/kernfs
make
cd tests
make

set +x
