#!/bin/bash

# Need to pass the number of producer as argument
default=1
producer=${1:-$default}

# Specify the base directories for code and result
CODE=$DEVFSSRC
RESULT_BASE=$DEVFSSRC/result
result_dir=$RESULT_BASE/freqthrottle

APPPREFIX="numactl --membind=0 --cpunodebind=0"

# Setup Parameters
let QSIZE=1
let NUMFS=1
let DELETEFILE=0
let IOSIZE=4096
let JOURNAL=0
let KERNIO=0

let READERS=-1
let WRITERS=-1
let SCHED=0
let DEVCORECOUNT=1
let QUEUEDEPTH=32

let MAX_READER=16
let MAX_WRITER=4

FILESIZE="8G"
FILENAME="devfile15"
FSPATH=/mnt/ram


# Create output directories
if [ ! -d "$result_dir" ]; then
        mkdir $result_dir
fi

if [ ! -d "$result_dir/1.2" ]; then
	mkdir $result_dir/1.2
fi

if [ ! -d "$result_dir/1.8" ]; then
	mkdir $result_dir/1.8
fi

if [ ! -d "$result_dir/2.4" ]; then
	mkdir $result_dir/2.4
fi


sudo dmesg -c
cd $CODE

#sudo mkdir $FSPATH
#sudo chown -R $USER $FSPATH

if mount | grep $FSPATH > /dev/null; then
	echo "devfs already mounted"
else
	$CODE/mountdevfs.sh
fi

# Setup experiment argument list
ARGS="-j $JOURNAL -k $KERNIO -g $NUMFS -d $DELETEFILE -q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

# First fill up the test file
$CODE/crfs_client -f "$FSPATH/$FILENAME" $ARGS


SCHED=1
DEVCORECOUNT=4
READERS=8
WRITERS=8

# Scaling CPU freq to 1.2GHz
$CODE/scripts/cpufreq_scaling.sh 200000
ARGS="-j $JOURNAL -k $KERNIO -g $NUMFS -d $DELETEFILE -q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"	
$APPPREFIX $CODE/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/1.2/output.txt

# Scaling CPU freq to 1.8GHz
$CODE/scripts/cpufreq_scaling.sh 800000
ARGS="-j $JOURNAL -k $KERNIO -g $NUMFS -d $DELETEFILE -q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"	
$APPPREFIX $CODE/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/1.8/output.txt

# Scaling CPU freq to 2.4GHz
$CODE/scripts/cpufreq_scaling.sh 3000000
ARGS="-j $JOURNAL -k $KERNIO -g $NUMFS -d $DELETEFILE -q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"	
$APPPREFIX $CODE/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/2.4/output.txt

