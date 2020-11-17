#include "nv_stats.h"

#ifdef _NVSTATS
s_procstats proc_stat;
UINT total_mmaps;
struct timeval commit_start, commit_end;
#endif

UINT nvlib_cflushcntr;


#ifdef _NVSTATS

int clear_start(int pid) {
#ifdef _USE_CHECKPOINT
	proc_stat.tot_cmtdata = 0;
#endif
	nvlib_cflushcntr =0;
}

int add_stats_chunk(int pid, size_t chunksize) {

	proc_stat.pid = pid;
	proc_stat.tot_chunksz += chunksize;
	proc_stat.num_chunks++;
#ifdef _USE_CHECKPOINT
	proc_stat.chunk_dist[proc_stat.num_chunks] = chunksize;
#endif
	return 0;
}

int add_stats_chunk_read(int pid, size_t chunksize) {

	proc_stat.pid = pid;
#ifdef _USE_CHECKPOINT
	proc_stat.tot_rd_chunksz += chunksize;
	proc_stat.num_rd_chunks++;
#endif
	return 0;
}

int add_stats_mmap(int pid, size_t mmap_size) {

	proc_stat.pid = pid;
	proc_stat.num_mmaps++;
	proc_stat.tot_mmapsz += mmap_size;
}


int add_stats_commit_freq(int pid, long time) {

	proc_stat.pid = pid;
#ifdef _USE_CHECKPOINT
	proc_stat.commit_freq = time;
#endif
}

int add_stats_chkpt_time(int pid, long time) {

	proc_stat.pid = pid;
#ifdef _USE_CHECKPOINT
	proc_stat.per_step_chkpt_time = time;
#endif
}

int add_to_chunk_memcpy(chunkobj_s *chunk) {

	if(chunk);
	chunk->num_memcpy = chunk->num_memcpy +1;
}

void incr_proc_mmaps(){
	total_mmaps++;
}




int print_stats(int pid) {

	int i = 0;

	fprintf(stdout,"*************************\n");
	fprintf(stdout, "PID: %d \n",proc_stat.pid);
	fprintf(stdout, "NUM MMAPS: %d \n",proc_stat.num_mmaps);
	fprintf(stdout, "NUM CHUNKS: %d \n",proc_stat.num_chunks);
	fprintf(stdout, "MAPSIZE: %u \n",proc_stat.tot_mmapsz);
	fprintf(stdout, "CHUNKSIZE: %u \n",proc_stat.tot_chunksz);
//#ifdef _USE_CACHEFLUSH
	fprintf(stdout, "num_cache line flushes: %u \n",get_cflush_cntr());
//#endif

#ifdef _USE_CHECKPOINT
	fprintf(stdout, "CHKPT COUNT: %u \n",local_chkpt_cnt);
	fprintf(stdout, "TOT COMMIT SIZE: %u \n",proc_stat.tot_cmtdata);
	fprintf(stdout, "COMMIT FREQ %ld \n",proc_stat.commit_freq);
	fprintf(stdout, "TOT CHKPT TIME %ld \n",proc_stat.per_step_chkpt_time);
#endif

#ifdef _USE_CHECKPOINT
	fprintf(stdout, "TOT READ CHUNKSZ %u \n",proc_stat.tot_rd_chunksz);
	fprintf(stdout, "NUM READ CHUNKs COUNT %d \n",proc_stat.num_rd_chunks);
	fprintf(stdout, "CHUNKSIZE STAT: \t");
	for( i =0; i< proc_stat.num_chunks; i++){
		fprintf(stdout,"%u ",proc_stat.chunk_dist[i]);
	}
	print_all_chunks();
	print_adtnl_copy_overhead(local_chkpt_cnt);
	fprintf(stdout,"\n\n");
#endif
	/*reset incremeneting fields */
	clear_start(proc_stat.pid);
}

#endif

/*
 * Cache flush counter incr methods
 */
void addto_cflush_cntr(UINT value){
        nvlib_cflushcntr += value;
}
void incr_cflush_cntr(){
        nvlib_cflushcntr++;
}
UINT get_cflush_cntr(){
        return nvlib_cflushcntr;
}




