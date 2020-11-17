#! /bin/bash

set -x

cd $LIBFS
make clean


#First compile nvmlog
cd $NVMALLOC_HOME
make clean
make
sudo make install

cd $LIBFS
make clean
make 
sudo make install

cd $LIBFS/interval-tree
make clean
make
sudo make install

#compile shim
cd $LIBFS/libshim
make clean
./makeshim.sh

#compile benchmarks
cd $LIBFS/benchmark
make clean && make
set +x
