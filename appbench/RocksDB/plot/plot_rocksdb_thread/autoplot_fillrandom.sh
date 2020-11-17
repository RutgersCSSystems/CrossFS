#!/bin/bash

# create data file for zplot
rm thread_fillrandom.data
touch thread_fillrandom.data
echo "# thread ext4dax devfs parafs_naive parafs_direct" > thread_fillrandom.data

# extract data from result
declare -a threadarr=("1" "2" "4" "8")

LINE=2
for THREAD in "${threadarr[@]}"
do
	echo "$THREAD " >> thread_fillrandom.data	

	num="`grep -o 'fillrandom.*[0-9]* ops.sec' ../../result-ext4dax/fillrandom-readrandom_"$THREAD"_4096.txt | sed 's/fillrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" thread_fillrandom.data

	num="`grep -o 'fillrandom.*[0-9]* ops.sec' ../../result-devfs/fillrandom-readrandom_"$THREAD"_4096.txt | sed 's/fillrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" thread_fillrandom.data

	num="`grep -o 'fillrandom.*[0-9]* ops.sec' ../../result-parafs-ioctl/fillrandom-readrandom_"$THREAD"_4096.txt | sed 's/fillrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" thread_fillrandom.data

	num="`grep -o 'fillrandom.*[0-9]* ops.sec' ../../result-parafs-noioctl/fillrandom-readrandom_"$THREAD"_4096.txt | sed 's/fillrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" thread_fillrandom.data

	let LINE=LINE+1

done

# now plot the graph with zplot
python thread_fillrandom.py
