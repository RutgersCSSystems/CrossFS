#!/bin/bash

# Need to pass the number of producer as argument
default=4
producer=${1:-$default}

# Specify the base directories for code and result
CODE=$DEVFSSRC
RESULT_BASE=$BASE/results/microbench-$DEVICE/nova
result_dir=$RESULT_BASE/stat_hitrate

mkdir -p $result_dir


# Setup Parameters
let IOSIZE=4096

let READERS=-1
let WRITERS=-1
let SCHED=0
let DEVCORECOUNT=1
let QUEUEDEPTH=16

let MAX_READER=16
let MAX_WRITER=4

FILESIZE="64k"
FILENAME="devfile15"
FSPATH=/mnt/pmemdir


# Create output directories
if [ ! -d "$result_dir" ]; then
        mkdir -p $result_dir
fi

if [ ! -d "$result_dir/$producer" ]; then
	mkdir -p $result_dir/$producer
fi

# Create output directory for different number of consumers(readers)
i=1
while (( $i <= $MAX_READER ))
do
	if [ ! -d "$result_dir/$producer/$i" ]; then
		mkdir -p $result_dir/$producer/$i
	fi

	i=$((i*2))
done

sudo dmesg -c
cd $CODE

#sudo mkdir $FSPATH
#sudo chown -R $USER $FSPATH

if mount | grep $FSPATH > /dev/null; then
        echo "nova already mounted"
else
        $CODE/scripts/mountnova.sh
fi

# Setup experiment argument list
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

# First fill up the test file
$CODE/benchmark/crfs_client_posixio -f "$FSPATH/$FILENAME" $ARGS
sleep 2

SCHED=1
DEVCORECOUNT=4
READERS=1
WRITERS=4
MAX_QUEUEDEPTH=256
MAX_READER=16

# Vary the number of producer(writer)
while (( $WRITERS <= $MAX_WRITER ))
do
        READERS=1
        # Vary the number of consumer(reader)
        while (( $READERS <= $MAX_READER ))
        do
		ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE -c 1"

		mkdir -p  $result_dir/$WRITERS/$READERS
		#echo "Starting, writing to $result_dir/$WRITERS/$READERS/output.txt"
		$CODE/benchmark/crfs_client_stat_posixio -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/$WRITERS/$READERS/output.txt
		echo "Ending"
		#rm $FSPATH/$FILENAME
		sleep 2

                READERS=$((READERS*2))
        done
        WRITERS=$((WRITERS*2))
done

