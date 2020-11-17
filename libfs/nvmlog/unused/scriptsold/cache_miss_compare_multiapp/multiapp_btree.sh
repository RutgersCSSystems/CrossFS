sudo chmod o+rw /dev/cpu/*/msr

let i=1

for i in 1 to 10

do

likwid-perfctr  -C 0:7  -g L2_LINES_IN_THIS_CORE_ALL:PMC0 DTLB_LOAD_MISSES_ANY:PMC1  /home/hendrix/jan12_link/benchmarks/memory_benchmarks/stream 

	sleep 6

	/home/sudarsun/nvmalloc/benchmark_cacheeff_je 0 1500000 0 0 
	#/home/sudarsun/nvmalloc/benchmark 0 1500000 0 0 

	sleep 30 

	#kill `ps -ef | pgrep benchmark | grep -v grep | awk '{print $2}'`
#	i=$i+1;	
done


