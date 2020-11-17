#!/bin/bash

# Need to pass the number of producer as argument
default=4
producer=${1:-$default}

# Specify the base directories for code and result
CODE=$DEVFSSRC

# Setup Parameters
let IOSIZE=4096
let READERS=0
let WRITERS=4

# Use roundrobin scheduler
let SCHED=0 

let DEVCORECOUNT=4
let QUEUEDEPTH=32

let MAX_READER=16
let MAX_WRITER=4

FILESIZE="64K"
FILENAME="devfile15"
FSPATH=/mnt/ram

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
ARGS="-q $QUEUEDEPTH -s $IOSIZE -t $READERS -u $WRITERS -p $SCHED -v $DEVCORECOUNT -b $FILESIZE"

# First fill up the test file
$CODE/benchmark/crfs_client_openclose -f "$FSPATH/$FILENAME" $ARGS
#sudo rm -rf /mnt/ram/*
sleep 2
