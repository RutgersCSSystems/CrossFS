#!/bin/bash

# Need to pass the number of producer as argument
default=4
producer=${1:-$default}

# Specify the base directories for code and result
CODE=$DEVFSSRC
RESULT_BASE=$BASE/results/microbench-$DEVICE/ext4dax
result_dir=$RESULT_BASE/stat_conflict

mkdir -p $result_dir


# Setup Parameters
let IOSIZE=4096

let READERS=-1
let WRITERS=-1
let SCHED=0
let DEVCORECOUNT=1
let QUEUEDEPTH=16

let MAX_READER=16
let MAX_WRITER=16

FILESIZE="64k"
FILENAME="devfile15"
FSPATH=/mnt/pmemdir


# Create output directories
if [ ! -d "$result_dir" ]; then
        mkdir -p $result_dir
fi

# Create output directory for different number of consumers(readers)
i=1
while (( $i <= $MAX_WRITER ))
do
	if [ ! -d "$result_dir/$i" ]; then
		mkdir -p $result_dir/$i
	fi

	i=$((i*2))
done

sudo dmesg -c
cd $CODE

#sudo mkdir $FSPATH
#sudo chown -R $USER $FSPATH

if mount | grep $FSPATH > /dev/null; then
        echo "ext4 dax already mounted"
else
        $CODE/scripts/mountext4dax.sh
fi

# Setup experiment argument list
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

# First fill up the test file
$CODE/benchmark/crfs_client_posixio -f "$FSPATH/$FILENAME" $ARGS
sleep 2

SCHED=1
DEVCORECOUNT=4
READERS=0
WRITERS=1
MAX_QUEUEDEPTH=256
MAX_READER=16

# Vary the number of producer(writer)
while (( $WRITERS <= $MAX_WRITER ))
do
	ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE -c 0"

	mkdir -p  $result_dir/$WRITERS
	#echo "Starting, writing to $result_dir/$WRITERS/output.txt"
	$CODE/benchmark/crfs_client_stat_posixio -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/$WRITERS/output.txt
	echo "Ending"
	#rm $FSPATH/$FILENAME
	sleep 2

        WRITERS=$((WRITERS*2))
done

