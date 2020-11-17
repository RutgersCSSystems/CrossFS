#!/bin/bash

sudo chmod o+rw /dev/cpu/*/msr

let i=1

for i in 1 2 3 4 5 .. 10
do
	likwid-perfctr  -C 1:1  -g L2_LINES_IN_THIS_CORE_ALL:PMC0 ./picture_animate.sh >> $1 &
	#hash benchmark with transaction
	/home/sudarsun/nvmalloc/benchmark 0 1500000 0 0
	sleep 30
done

for i in 1 2 3 4 5 .. 10
do
	likwid-perfctr  -C 1:1  -g L2_LINES_IN_THIS_CORE_ALL:PMC0 ./picture_animate.sh >> $2 &
	#hash benchmark without transaction
	/home/sudarsun/nvmalloc/benchmark_cacheeff_je 0 1500000 0 0 
	sleep 30
done

