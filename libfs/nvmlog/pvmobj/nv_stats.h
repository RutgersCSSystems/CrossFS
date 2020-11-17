/*
 * nv_stats.h
 *
 *  Created on: Mar 21, 2013
 *      Author: hendrix
 */

#ifndef NV_STATS_H_
#define NV_STATS_H_


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <strings.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "nv_map.h"
#include "nv_def.h"




int clear_start(int pid);
int add_stats_chunk(int pid, size_t chunksize);
int add_stats_chunk_read(int pid, size_t chunksize);
int add_stats_mmap(int pid, size_t mmap_size);
int add_stats_commit_freq(int pid, long time);
int add_stats_chkpt_time(int pid, long time);
int add_to_chunk_memcpy(chunkobj_s *chunk);
int print_stats(int pid);
void incr_proc_mmaps();

void addto_cflush_cntr(UINT value);
void incr_cflush_cntr();
UINT get_cflush_cntr();



#endif /* NV_STATS_H_ */
