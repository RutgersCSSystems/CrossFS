#! /bin/bash
set -x

PARAFS=$OFFLOADBASE
DBPATH=/mnt/ram
BENCHMARK=fillseq,readseq
WORKLOADDESCR="fillseq-readseq"

RESULTDIR=$1

# Create output directories
if [ ! -d "$RESULTDIR" ]; then
	mkdir $RESULTDIR
fi

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
	export PARAFSENV=parafs
	export DEVCORECNT=4
	export SCHEDPOLICY=2
	cd $ROCKSDB
	LD_PRELOAD=$LIBFS/libshim/shim.so $ROCKSDB/db_bench --db=$DBPATH --num_levels=6 --key_size=20 --prefix_size=20 --memtablerep=prefix_hash --bloom_bits=10 --bloom_locality=1 --benchmarks=$BENCHMARK --use_existing_db=0 --num=200000 --compression_type=none --value_size=$2 --threads=$1  &> $RESULTDIR/$WORKLOADDESCR"_"$1"_"$2".txt"
#./db_bench --db=$PWD/DATA --num_levels=6 --key_size=20 --prefix_size=12 --keys_per_prefix=10 --value_size=100 --cache_size=17179869184 --cache_numshardbits=6 --compression_type=none --compression_ratio=1 --min_level_to_compress=-1 --disable_seek_compaction=1 --hard_rate_limit=2 --write_buffer_size=134217728 --max_write_buffer_number=2 --level0_file_num_compaction_trigger=8 --target_file_size_base=134217728 --max_bytes_for_level_base=1073741824 --disable_wal=0 --wal_dir=$PWD/WAL/WAL_LOG --verify_checksum=1 --delete_obsolete_files_period_micros=314572800 --max_background_compactions=4 --max_background_flushes=0 --level0_slowdown_writes_trigger=16 --level0_stop_writes_trigger=24 --histogram=0 --use_plain_table=1 --open_files=-1 --mmap_read=1 --mmap_write=0 --bloom_bits=10 --bloom_locality=1 --benchmarks= readrandom --num=500000 --threads=4
	unset PARAFSENV
	sleep 2
}

declare -a sizearr=("100" "512" "1024" "4096")
#declare -a sizearr=("100")
declare -a threadarr=("1" "2" "4" "8")
for size in "${sizearr[@]}"
do
	for thrd in "${threadarr[@]}"
	do
	        CLEAN
		FlushDisk
		RUN $thrd $size
	done
done
