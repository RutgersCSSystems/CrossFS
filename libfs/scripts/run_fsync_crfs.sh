#!/bin/bash

default=1
producer=${1:-$default}

# Specify the base directories for code and result
CODE=$DEVFSSRC

RESULT_BASE=$BASE/results/microbench-$DEVICE/parafs-noioctl
result_dir=$RESULT_BASE/fsync
mkdir -p $result_dir


# Setup Parameters
let IOSIZE=4096

let READERS=-1
let WRITERS=-1
let SCHED=0
let DEVCORECOUNT=1
let QUEUEDEPTH=32
let FSYNCFREQ=0

let MAX_DEVCORECNT=4
let MAX_QUEUEDEPTH=32
let MAX_FSYNCFREQ=64

FILESIZE="12G"
FILENAME="devfile15"
FSPATH=/mnt/ram


# Create output directories
if [ ! -d "$result_dir" ]; then
        mkdir $result_dir
fi

# Create directory for different queue depth and fsync frequency
if [ ! -d "$result_dir/0" ]; then
	mkdir $result_dir/0
fi

i=1
while (( $i <= $MAX_FSYNCFREQ ))
do		
	if [ ! -d "$result_dir/$i" ]; then
		mkdir $result_dir/$i
	fi
	i=$((i*2))
done


sudo dmesg -c
cd $CODE

#sudo mkdir $FSPATH
#sudo chown -R $USER $FSPATH
#if mount | grep $FSPATH > /dev/null; then
#	echo "crfs already mounted"
#else
#	$CODE/mountcrfs.sh
#fi


# Setup experiment argument list
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -a $FSYNCFREQ -b $FILESIZE"


# First fill up the test file
$CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS


let READERS=0
let WRITERS=8
let SCHED=0
let DEVCORECOUNT=$MAX_DEVCORECNT
let QUEUEDEPTH=$MAX_QUEUEDEPTH

#FSYNCFREQ=64
#ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -a $FSYNCFREQ -b $FILESIZE"
#$CODE/benchmark/crfs_client_fsync -f "$FSPATH/$FILENAME" $ARGS
#exit

# Get the baseline - No fsync
FSYNCFREQ=0
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -a $FSYNCFREQ -b $FILESIZE"
$CODE/benchmark/crfs_client_fsync -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/0/output.txt


# Run with different fsync frequency
FSYNCFREQ=4
while (( $FSYNCFREQ <= $MAX_FSYNCFREQ ))
do
	# Increase fsync frequency, FSYNCREQ = X refers to do 1 fsyc every X writes
	ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -a $FSYNCFREQ -b $FILESIZE"

	$CODE/benchmark/crfs_client_fsync -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/$FSYNCFREQ/output.txt

	FSYNCFREQ=$((FSYNCFREQ*2))
done


