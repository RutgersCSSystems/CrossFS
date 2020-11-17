#!/bin/bash

# create data file for zplot
rm valuesize_readrandom.data
touch valuesize_readrandom.data
echo "# thread ext4dax devfs parafs_naive parafs_direct" > valuesize_readrandom.data

# extract data from result
declare -a sizearr=("100" "512" "1024" "4096")

LINE=2
for VALUE in "${sizearr[@]}"
do
	echo "$VALUE " >> valuesize_readrandom.data	

	num="`grep -o 'readrandom.*[0-9]* ops.sec' ../../result-ext4dax/fillrandom-readrandom_8_"$VALUE".txt | sed 's/readrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" valuesize_readrandom.data

	num="`grep -o 'readrandom.*[0-9]* ops.sec' ../../result-devfs/fillrandom-readrandom_8_"$VALUE".txt | sed 's/readrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" valuesize_readrandom.data

	num="`grep -o 'readrandom.*[0-9]* ops.sec' ../../result-parafs-ioctl/fillrandom-readrandom_8_"$VALUE".txt | sed 's/readrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" valuesize_readrandom.data

	num="`grep -o 'readrandom.*[0-9]* ops.sec' ../../result-parafs-noioctl/fillrandom-readrandom_8_"$VALUE".txt | sed 's/readrandom.*op //'1 | sed 's/ ops.sec//'1`"
	num=$((num/1000))
	sed -i -e "$LINE s/$/ $num/" valuesize_readrandom.data

	let LINE=LINE+1

done

# now plot the graph with zplot
python valuesize_readrandom.py
