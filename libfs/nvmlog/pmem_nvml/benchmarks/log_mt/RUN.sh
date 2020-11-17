#! /bin/bash

OPS_COUNT=1000000
MAX_THREADS=1
RUNS=`seq $MAX_THREADS`
LOG_IN="/mnt/pmfs/log_mt.tmp"
PMEMLOG_OUT=/mnt/pmfs/pmemlog_mt.out
FILEIOLOG_OUT=/mnt/pmfs/fileiolog_mt.out

rm -f $FILEIOLOG_OUT
for i in $RUNS ; do
	./log_mt -i -v 1 -e 8192 $i $OPS_COUNT $LOG_IN >> $FILEIOLOG_OUT;
	rm -f $LOG_IN
done

rm -f $PMEMLOG_OUT
for i in $RUNS ; do
	./log_mt -v 1 -e 8192 $i $OPS_COUNT $LOG_IN >> $PMEMLOG_OUT;
	rm -f $LOG_IN
done

gnuplot gnuplot_log_mt_append.p
gnuplot gnuplot_log_mt_read.p
