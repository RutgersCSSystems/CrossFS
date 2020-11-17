#! /bin/bash
set -x

#DBPATH=$STORAGEPATH
DBPATH=/mnt/pmemdir
VALUSESIZE=100
BENCHMARK="fillrandom,readrandom"
WORKLOADDESCR="fillrandom-readrandom"

RESULT_BASE=$BASE/results/apps-$DEVICE/ext4dax
result_dir=$RESULT_BASE/rocksdb/
mkdir -p $result_dir

# Create output directories
#if [ ! -d "result-ext4dax-merged" ]; then
#	mkdir result-ext4dax-merged
#fi

CLEAN() {
	rm -rf /mnt/ram/*
	sudo killall "db_bench"
	sudo killall "db_bench"
	echo "KILLING Rocksdb db_bench"
}

FlushDisk() {
	sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
	sudo sh -c "sync"
	sudo sh -c "sync"
	sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
}

RUN() {
	#mkdir -p $result_dir/$WORKLOADDESCR_$1_$2
	./db_bench --db=$DBPATH --num_levels=6 --key_size=20 --prefix_size=20 --memtablerep=prefix_hash --bloom_bits=10 --bloom_locality=1 --benchmarks=$BENCHMARK --use_existing_db=0 --num=500000 --compression_type=none --value_size=$2 --threads=$1  &> $result_dir/$WORKLOADDESCR"_"$1"_"$2".txt"
	sleep 2
}

#declare -a sizearr=("100" "512" "1024" "4096") 
declare -a sizearr=("1024")
declare -a threadarr=("1" "2" "4" "8" "16")
for size in "${sizearr[@]}"
do
	for thrd in "${threadarr[@]}"
	do
		CLEAN
		FlushDisk
		RUN $thrd $size
	done        
done   
