#!/bin/bash

# Need to pass the number of producer as argument
default=4
producer=${1:-$default}

# Specify the base directories for code and result
CODE=$DEVFSSRC

RESULT_BASE=$BASE/results/microbench-$DEVICE/parafs-noioctl
result_dir=$RESULT_BASE/concurrency-multi-proc
mkdir -p $result_dir


# Setup Parameters
let IOSIZE=4096
let READERS=-1
let WRITERS=-1

# Use roundrobin scheduler
let SCHED=2

let DEVCORECOUNT=1
let QUEUEDEPTH=32

let MAX_READER=4
let MAX_WRITER=4

FILESIZE="12G"
FILENAME="devfile15"
FSPATH=/mnt/ram

# Create output directories
if [ ! -d "$result_dir" ]; then
        mkdir -p $result_dir
fi

if [ ! -d "$result_dir/writers" ]; then
        mkdir -p $result_dir/writers
fi

if [ ! -d "$result_dir/readers" ]; then
        mkdir -p $result_dir/writers
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

#create shared memory in ramfs
sudo mkdir -p /mnt/tmpfs
sudo mount -t ramfs -o size=512m ramfs /mnt/tmpfs


# Setup experiment argument list
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

# First fill up the test file
$CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS
sleep 2

SCHED=2
DEVCORECOUNT=1

# Creating writer processes
READERS=0
WRITERS=1
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"
let i=1
while (( $i <= $MAX_WRITER ))
do
        mkdir -p $result_dir/writers/$i
        echo "Starting, writing to $result_dir/writers/$i/output.txt"
        $CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/writers/$i/output.txt &
        i=$((i+1))
done

# Creating reader processes
READERS=1
WRITERS=0
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"
let i=1
while (( $i <= $MAX_READER ))
do
        mkdir -p $result_dir/readers/$i
        echo "Starting, writing to $result_dir/readers/$i/output.txt"
        $CODE/benchmark/crfs_client -f "$FSPATH/$FILENAME" $ARGS &> $result_dir/readers/$i/output.txt &
        i=$((i+1))
done

sleep 20
