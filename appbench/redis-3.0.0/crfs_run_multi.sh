#!/bin/bash
set -x

REDISCONF=$APPBENCH/redis-3.0.0/redis-conf
REDISDIR=$APPBENCH/redis-3.0.0/src
DBDIR=$STORAGEPATH

let MAXINST=8
let STARTPORT=6378
let KEYS=100000

PARAFS=$BASE
DIR=""
RESULT_BASE=$BASE/results/apps-$DEVICE/parafs-noioctl
result_dir=$RESULT_BASE/redis
mkdir -p $result_dir


# Create output directories
if [ ! -d "$result_dir" ]; then
	mkdir -p $result_dir
fi

CLEAN() {
	for (( b=1; b<=$MAXINST; b++ ))
	do
		rm -rf $DBDIR/*.rdb
		rm -rf $DBDIR/*.aof
		sudo pkill "redis-server$b"
		echo "KILLING redis-server$b"
	done

}

PREPARE() {
	for (( inst=1; inst<=$MAXINST; inst++ ))
	do
		cp $REDISDIR/redis-server $REDISDIR/redis-server$inst
	done
}

FlushDisk() {
        sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
        sudo sh -c "sync"
        sudo sh -c "sync"
        sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
}

RUN_SERVER(){

        export PARAFSENV=parafs
        export DEVCORECNT=1
	export MULPROC=1
	echo "MAXINST... $MAXINST"
	let port=$STARTPORT
	for (( r=1; r<=$MAXINST; r++))
	do
		LD_PRELOAD=$LIBFS/libshim/shim.so  $REDISDIR/redis-server$r $REDISCONF/redis-$port".conf" &
		#$REDISDIR/redis-server$r $REDISCONF/redis-$port".conf" &
		let port=$port+1
	done
	export LD_PRELOAD=""
}

RUN_CLIENT(){
	let port=$STARTPORT

	for (( c=1; c<$MAXINST; c++))
	do
		mkdir -p $DIR/$c

		numactl --membind=0 $REDISDIR/redis-benchmark -t set -p $port -q -n $KEYS -d $1  &> $DIR/$c/$1".txt" &
		let port=$port+1
	done

	mkdir -p $DIR/$c
	$REDISDIR/redis-benchmark -t set -p $port -q -n $KEYS -d $1  &> $DIR/$c/$1".txt"
	sleep 5
}

TERMINATE() {
	for (( c=1; c<=$MAXINST; c++))
	do
		sudo pkill redis-server$i
		sudo pkill redis-server$i
	done
	sudo pkill redis-benchmark
	#$LIBFS/scripts/cleanup.sh
	sleep 5
}


CLEAN
PREPARE

for i in 1 2 4 8
do
	echo "Going to sleep waiting for redis servers to terminate gracefully"
	sleep 5

	let MAXINST=$i

	DIR=$result_dir/$MAXINST"-inst"
	mkdir -p $DIR

	CLEAN
	FlushDisk
	RUN_SERVER
	sleep 8
	RUN_CLIENT 1024
	TERMINATE
done

CLEAN
echo "Finished all the test, going to sleep 16 sec waiting for redis servers to terminate gracefully"
sleep 10
sudo pkill -9 parafs_run*

