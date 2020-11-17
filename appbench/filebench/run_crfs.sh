#!/bin/bash

FSPATH=/mnt/ram
DEVFSSRC=$BASE

RESULT_BASE=$BASE/results/apps-$DEVICE/parafs
result_dir=$RESULT_BASE/filebench
mkdir -p $result_dir

set -x

# Create output directories
if [ ! -d "$RESULT_BASE" ]; then
        mkdir $RESULT_BASE
fi

if [ ! -d "$result_dir" ]; then
        mkdir $result_dir
fi


if mount | grep $FSPATH > /dev/null; then
    echo "devfs already mounted"
else
	$DEVFSCLIENT/scripts/mountdevfs.sh
fi

echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

export PARAFS=parafs
export DEVCORECNT=4
export SCHEDPOLICY=2

$FILEBENCH/filebench -f $FILEBENCH/myworkloads/webserver_parafs.f
$FILEBENCH/filebench -f $FILEBENCH/myworkloads/fileserver_parafs.f
$FILEBENCH/filebench -f $FILEBENCH/myworkloads/varmail_parafs.f

unset PARAFS

set +x
