#!/bin/bash

# This experiment is aimed to show how scheduler performs
# with equal number of readers and writes
# Idealy, round-robin will also starve both reader and writer.

CODE=$DEVFSSRC

RESULT_BASE=$BASE/results/microbench-$DEVICE/parafs-noioctl
result_dir=$RESULT_BASE/scheduler
mkdir -p $result_dir


# Setup Parameters
let IOSIZE=4096

let READERS=-1
let WRITERS=-1
let SCHED=0
let DEVCORECOUNT=1
let QUEUEDEPTH=1

let MAX_READER=16
let MAX_DEVCORECNT=4

FILESIZE="2G"
FILENAME="devfile15"
FSPATH=/mnt/ram


# Create output directories
if [ ! -d "$result_dir" ]; then
	mkdir $result_dir
fi


# Create directory for experiment iterations
if [ ! -d "$result_dir/rr" ]; then
	mkdir $result_dir/rr
fi

if [ ! -d "$result_dir/rp" ]; then
	mkdir $result_dir/rp
fi

# Create directory for different configurations

j=1
while (( $j <= $MAX_READER ))
do
	if [ ! -d "$result_dir/rr/$j" ]; then
		mkdir $result_dir/rr/$j
	fi

	if [ ! -d "$result_dir/rp/$j" ]; then
		mkdir $result_dir/rp/$j
	fi

	j=$((j*2))
done


sudo dmesg -c
cd $CODE

if mount | grep $FSPATH > /dev/null; then
	echo "crfs already mounted"
	#$CODE/mountcrfs.sh
fi

# Setup experiment argument list
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"


# First fill up the test file
$CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS


# different number of consumers and producers running Round Robin
SCHED=0	# Round Robin
DEVCORECOUNT=4
# Vary the number of device CPU count

READERS=1
# Vary the number of producer(writer)
while (( $READERS <= $MAX_READER ))
do
	WRITERS=$READERS
	# Vary the number of producer(writer)
	ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

	$CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/rr/$READERS/output.txt
	sudo dmesg -c >> $result_dir/rr/$READERS/output.txt

	# Clear page cache
	echo 3 | sudo tee /proc/sys/vm/drop_caches
	sleep 1

	READERS=$((WRITERS*2))
done


# different number of consumers and producers running Round Robin
SCHED=1	# Read Prioritized Deadline
DEVCORECOUNT=4

READERS=1
# Vary the number of producer(writer)
while (( $READERS <= $MAX_READER ))
do
	WRITERS=$READERS
	# Vary the number of producer(writer)
	ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

	$CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/rp/$READERS/output.txt
	sudo dmesg -c >> $result_dir/rp/$READERS/output.txt

	# Clear page cache
	echo 3 | sudo tee /proc/sys/vm/drop_caches
	sleep 1

	READERS=$((WRITERS*2))
done

