#include "hash_maps.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <map>
#include <signal.h>
#include <queue>
#include <list>
#include <algorithm>
#include <functional>
#include <pthread.h>
#include <sys/time.h>
#include <unordered_map>
#include "nv_map.h"
#include "nv_def.h"

using namespace std;

std::unordered_map <int, size_t> proc_vmas;
std::unordered_map <int, size_t> metadata_vmas;
std::unordered_map<int, size_t>::iterator vma_itr;
std::unordered_map <void *, chunkobj_s *> chunkmap;
std::unordered_map <void *, chunkobj_s *>::iterator chunk_itr;
std::unordered_map<int, chunkobj_s *> id_chunk_map;


#ifdef _NVRAM_OPTIMIZE
#define CACHE_ENTRIES 8
UINT vmaid_cache[CACHE_ENTRIES];
UINT vmasize_cache[CACHE_ENTRIES];
UINT next_vma_entry;
#endif


void* get_chunk_from_map(void *addr) {

	size_t bytes = 0;
	std::map <int, chunkobj_s *>::iterator id_chunk_itr;
	unsigned long ptr = (unsigned long)addr;
    unsigned long start, end;
	
    for( chunk_itr= chunkmap.begin(); chunk_itr!=chunkmap.end(); ++chunk_itr){
        chunkobj_s * chunk = (chunkobj_s *)(*chunk_itr).second;
        bytes = chunk->length;
		start = (ULONG)(*chunk_itr).first;
		end = start + bytes;
#ifdef NV_DEBUG
		//fprintf(stderr,"fetching %ld start %ld end %ld\n",ptr, start, end);
#endif
	    /*if(end) {
    	    unsigned long off = 0;
        	off = 4096- (((unsigned long)end % 4096));
	        fprintf(stdout,"off %d \n", off);
			end = end + off;
	   }*/
		if( ptr >= start && ptr <= end) {
			return (void *)chunk;
		}
	}
	return NULL;
}

void* get_chunk_with_id(UINT chunkid){

	return (void *)id_chunk_map[chunkid];
}


int record_chunks(void* addr, chunkobj_s *chunk) {

	chunkmap[addr] = chunk;
	//fprintf(stdout,"record_chunks \n");
	//id_chunk_map[chunk->chunkid] = chunk;
	return 0;
}

int get_chnk_cnt_frm_map() {

	return chunkmap.size();
}

//Assuming that chunkmap can get data in o(1)
//Memory address range will not work here
//If this method returns NULL, caller needs
//to check if addr is in allocated ranger using
//the o(n) get_chunk_from_map call
void *get_chunk_from_map_o1(void *addr) {

	chunkobj_s *chunk;
	assert(chunkmap.size());
	assert(addr);
	chunk = ( chunkobj_s *)chunkmap[addr];
	return (void *)chunk;
}

int record_metadata_vma(int vmaid, size_t size) {
    metadata_vmas[vmaid] = size;
    return 0;
}

int record_vmas(int vmaid, size_t size) {

    proc_vmas[vmaid] = size;

#ifdef _NVRAM_OPTIMIZE
	vmaid_cache[next_vma_entry] = vmaid;
	vmasize_cache[next_vma_entry] = size;
	next_vma_entry++;
	next_vma_entry = next_vma_entry % CACHE_ENTRIES;
#endif

    return 0;
}

size_t get_vma_size(int vmaid){

	int idx =0;

#ifdef _NVRAM_OPTIMIZE
	for(idx=0; idx < CACHE_ENTRIES; idx++){
		if(vmaid_cache[idx] == vmaid){
			return vmasize_cache[idx];
		}
	}
#endif
	return proc_vmas[vmaid];
}


