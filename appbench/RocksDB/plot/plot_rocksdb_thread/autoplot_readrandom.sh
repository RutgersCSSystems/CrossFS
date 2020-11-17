#!/bin/bash

BASE=$ROCKSDB/plot/plot_rocksdb_thread
cd $BASE

# create data file for zplot
rm thread_readrandom.data
touch thread_readrandom.data
echo "# thread ext4dax devfs parafs_naive parafs_direct" > thread_readrandom.data

RESULTSBASE=$ROCKSDB
DAXRES="result-ext4dax"
DEVFSRES="result-devfs"
PARAFSRES="result-parafs-ioctl"
PARAFSDIRRES="result-parafs-noioctl"
VALSIZE="4096"

# extract data from result
declare -a threadarr=("1" "2" "4" "8")

LINE=2
for THREAD in "${threadarr[@]}"
do
	echo "$THREAD " >> thread_readrandom.data	

	resfile="fillrandom-readrandom_"$THREAD"_"$VALSIZE".txt"

	for config in $DAXRES $DEVFSRES $PARAFSRES $PARAFSDIRRES
	do
		num="`grep -o 'readrandom.*[0-9]* ops.sec' $RESULTSBASE/$config/$resfile | sed 's/readrandom.*op //'1 | sed 's/ ops.sec//'1`"
		num=$((num/1000))
		sed -i -e "$LINE s/$/ $num/" thread_readrandom.data
	done
	let LINE=LINE+1

done

# now plot the graph with zplot
python $BASE/thread_readrandom.py
