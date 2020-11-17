sudo chmod o+rw /dev/cpu/*/msr

let i=1

for i in 1 to 10

do
	likwid-perfctr  -C 1:1  -g L2_LINES_IN_THIS_CORE_ALL:PMC0  /home/sudarsun/apps/x264/x264 --output /dev/null --fps 100 --input-res 1080x1080  /home/sudarsun/apps/x264/18MB.mp4 >> $1 &

	sleep 2	

	#hash benchmark without transactions
	#/home/sudarsun/nvmalloc/benchmark_cacheeff_je 0 1500000 0 0 

	#hash benchmark with transaction
	/home/sudarsun/nvmalloc/benchmark 0 1500000 0 0 

	sleep 30

	#kill `ps -ef | pgrep benchmark | grep -v grep | awk '{print $2}'`
#	i=$i+1;	
done

#INSTR_RETIRED_ANY, 0x0, 0x0, FIXC0
#likwid-perfctr  -C 1:1  -g INSTR_RETIRED_ANY:FIXC0 
