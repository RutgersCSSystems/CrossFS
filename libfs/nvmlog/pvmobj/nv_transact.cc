/*
 * nv_transactions.cc
 *
 *  Created on: Mar 18, 2013
 *      Author: sudarsun@gatech.edu
 */

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
//#include "checkpoint.h"
#include "util_func.h"
#include "time_delay.h"
#include "jemalloc/jemalloc.h"
#include "LogMngr.h"
#include "nv_transact.h"
#include "nv_stats.h"

#ifdef _ENABLE_INTEL_LOG
#include <libpmemlog.h>
#endif


#define CACHESIZE 32

ULONG addr_cache[CACHESIZE];
ULONG chunk_cache[CACHESIZE];
UINT next_entry;
uint8_t initCache;


#ifdef _USE_TRANSACTION

void init_cache(){

	int idx=0;
	for(idx = 0; idx < CACHESIZE; idx++){
		addr_cache[idx]=0;
		chunk_cache[idx]=0;
	}
}

void add_to_cache(ULONG key, ULONG value){

	if(!initCache){
		init_cache();
		initCache++;
	}
	addr_cache[next_entry]=key;
	chunk_cache[next_entry]=value;
	next_entry++;
	next_entry = next_entry % CACHESIZE;
}

ULONG get_frm_cache(ULONG key){

	int idx=0;
	for(idx = 0; idx < CACHESIZE; idx++){
		if( addr_cache[idx]== key){
			//fprintf(stdout,"cache hit\n");
			return chunk_cache[next_entry];
		}
	}
	return 0;
}


int nv_commit_word(void *objptr) {

#ifdef _USE_REDO_LOG
	return 0;
#endif

	flush_cache(objptr, CACHE_LINE_SIZE);

	update_oncommit(objptr,1);
	return 0;
}

int nv_commit_obj(void *objptr) {

	chunkobj_s *chunk=NULL;
	void* dest = NULL;
	void *src = NULL;
	int result =0;
	//fprintf(stdout, "commtting obj \n");

#ifdef _USE_REDO_LOG
	return 0;
#endif

#ifdef _USE_UNDO_LOG
	/*find the chunk corresponding to addr*/
	chunk = (chunkobj_s *)get_frm_cache((ULONG)objptr);
#endif

	if(!chunk){
		chunk = (chunkobj_s *)get_chunk_from_map_o1(objptr);
	}
	if(!chunk) {
		fprintf(stdout,"chunk_from_map_o1 failed %lu\n",
							objptr);
		chunk = (chunkobj_s *)get_chunk_from_map(objptr);
	}
	assert(chunk);
	assert(chunk->length);

	//call Log manager to update fields
	update_oncommit(objptr,0);


#ifdef _USE_SHADOWCOPY
	src = chunk->log_ptr;
	dest =chunk->nv_ptr;
#else
	src = chunk->nv_ptr;
	dest = chunk->nv_ptr;
#endif
	assert(src);
	assert(dest);

#ifdef _USE_TRANSACTION
	int idx =0;

#ifdef _USE_UNDO_LOG

	#ifdef _NVSTATS
		incr_cflush_cntr();
	#endif

	flush_cache(dest, chunk->length);
#endif

#else
	memcpy(dest, src, chunk->length);
#endif

#ifdef _NVDEBUG
	fprintf(stderr,"nv_commit: COMPLETE \n");
#endif
	return 0;
}

int nv_begintrans_wrd(void* wrdptr, size_t size) {

	uint8_t isWordLogging = 1;

#ifdef _USE_REDO_LOG
        return 0;
#endif

#ifdef _USE_UNDO_LOG
	log_record(wrdptr, size, 0, isWordLogging);
#endif
}


int nv_begintrans_obj(void* objptr) {

	chunkobj_s *chunk=NULL;
	void* dest = NULL;
	void *src = NULL;
	int result = -1;
	uint8_t isWordlog = 0;

#ifdef _USE_REDO_LOG
        return 0;
#endif

#ifdef _NVDEBUG
	fprintf(stdout,"\n nv_map.ccnv_begintrans_obj %lu\n",
			(unsigned long)objptr);
#endif
	chunk = (chunkobj_s *)get_chunk_from_map_o1(objptr);
	if(!chunk) {
		fprintf(stdout,"get_chunk_from_map_o1 failed %lu\n", objptr);
		chunk = (chunkobj_s *)get_chunk_from_map(objptr);
	}
	assert(chunk);
	assert(chunk->length);
	//lock the metada

#ifdef _USE_TRANSACTION
	gt_spin_lock(&chunk->chunk_lock);
#endif

	//Copy the actual NVM data to log
#ifndef _USE_UNDO_LOG
	src = chunk->nv_ptr;
	dest =chunk->log_ptr;

	if(src ==  NULL || dest == NULL){
#ifdef _USE_TRANSACTION
		gt_spin_unlock(&chunk->chunk_lock);
#endif
		assert(0);
	}
#endif

#ifdef _USE_UNDO_LOG
	//if(chunk->dirty)
	{
		log_record(chunk->nv_ptr, chunk->length, chunk->chunkid, isWordlog);
	}
	add_to_cache((ULONG)chunk->nv_ptr, (ULONG)chunk);
#else
	memcpy(dest, src, chunk->length);
	flush_cache(dest,chunk->length);
#endif
	//release chunk lock
	gt_spin_unlock(&chunk->chunk_lock);

#ifdef _NVDEBUG
	fprintf(stderr,"initiated obj based transaction\n");
#endif
}

int nv_recover_obj(void *objptr) {

	chunkobj_s *chunk=NULL;
	void* dest = NULL;
	void *src = NULL;

	//find the chunk corresponding to addr
	chunk = (chunkobj_s *)get_chunk_from_map_o1(objptr);
	if(!chunk) {
		fprintf(stdout,"get_chunk_from_map_o1 failed\n");
		chunk = (chunkobj_s *)get_chunk_from_map(objptr);
	}
	assert(chunk);
	assert(chunk->length);

#ifdef _USE_SHADOWCOPY
	src = chunk->log_ptr;
	dest =chunk->nv_ptr;
#else
	src = chunk->nv_ptr;
	dest = chunk->nv_ptr;
#endif
	assert(src);
	assert(dest);

	memcpy(dest, src, chunk->length);
#ifdef _NVDEBUG
	fprintf(stderr,"nv_commit: COMPLETE \n");
#endif
	return 0;
}

//////////////////////////////REDO LOG ///////////////////////////////

int nvcommit_noarg(void) {
    commit_all();
}

//adds a addr and word to log
int store_log(nvword_t *addr, nvword_t value){
	return add_redo_word_record(addr, value);	
}

//reads a word from log
nvword_t load_log(nvword_t *addr){

	return read_log(addr);
}
//////////////////////////////////////////////////////////////////////


int nv_sync_obj(void *objptr) {

	chunkobj_s *chunk=NULL;

	/*find the chunk corresponding to addr*/
	chunk = (chunkobj_s *)get_frm_cache((ULONG)objptr);
	if(!chunk){
		chunk = (chunkobj_s *)get_chunk_from_map_o1(objptr);
	}
	if(!chunk) {
		fprintf(stdout,"chunk_from_map_o1 failed %lu\n",
							objptr);
		chunk = (chunkobj_s *)get_chunk_from_map(objptr);
	}
	assert(chunk);
	assert(chunk->length);
	flush_cache(chunk->nv_ptr, chunk->length);
	return 0;
}



void print_trans_stats() {
	print_log_stats();
}


#endif //_USE_TRANSACTION


#ifdef _ENABLE_INTEL_LOG

static PMEMlogpool *plp= NULL;
void *logdataddr=NULL;

int nv_initialize_log(void *addr)
{
	size_t nbyte;

	logdataddr = mmap(0, (size_t)TRANS_DATA_LOGSZ, PROT_NV_RW, MAP_PRIVATE|MAP_ANONYMOUS,-1, 0);


#ifdef _USE_FAKE_NVMAP

	int fd;
	char *str;


	/* create file on PMEM-aware file system */
	if ((fd = open(PROCLOG_DATA_PATH, O_CREAT|O_RDWR, 0666)) < 0) {
		perror("open");
		exit(1);
	}
	/* pre-allocate 2GB of persistent memory */
	if ((errno = posix_fallocate(fd, (off_t)0,
					(size_t)TRANS_DATA_LOGSZ)) != 0) {
		perror("posix_fallocate");
		exit(1);
	}
	close(fd);

	/* create a persistent memory resident log */
	if ((plp = pmemlog_pool_open(PROCLOG_DATA_PATH)) == NULL) {
		perror("pmemlog_pool_open");
		exit(1);
	}
#else
	plp = pmemlog_pool_open_nvm(logdataddr, PROCLOG_DATA_PATH, (size_t)TRANS_DATA_LOGSZ);
	//plp = pmemlog_pool_open(PROCLOG_DATA_PATH);
#endif
	/* how many bytes does the log hold? */
	nbyte = pmemlog_nbyte(plp);
	printf("log holds %zu bytes\n", nbyte);

}

int nv_append_log(void *addr, size_t size){

	if (pmemlog_append(plp, addr,size) < 0) {
		perror("pmemlog_append");
		exit(1);
	}
	/* print the log contents */
	printf("log contains:\n");
	//pmemlog_walk(plp, 0, printit, NULL);
	//pmemlog_pool_close(plp);
}

#endif


int nv_sync_chunk(void *objptr, size_t len) {

	assert(len);
	flush_cache(objptr, len);

#ifdef _ENABLE_INTEL_LOG
	nv_append_log(objptr, len);
#endif

	return 0;
}


