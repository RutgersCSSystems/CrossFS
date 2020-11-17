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
#include <limits.h>
#include <unordered_map>
#include <sstream>
#include "malloc_hook.h"
#include "nv_map.h"
#include "nv_def.h"
//#include "checkpoint.h"


using namespace std;

std::unordered_map <int, size_t> proc_vmas;
std::unordered_map <int, size_t> metadata_vmas;
std::unordered_map<int, size_t>::iterator vma_itr;
std::unordered_map <void *, chunkobj_s *> chunkmap;
std::unordered_map <void *, chunkobj_s *>::iterator chunk_itr;
std::unordered_map<int, chunkobj_s *> id_chunk_map;

std::unordered_map <unsigned long, size_t> allocmap;
std::unordered_map <unsigned long, size_t> alloc_prot_map;
std::map <void *, size_t> life_map;
//std::map <void *, size_t> fault_stat;
std::unordered_map <unsigned long, size_t>::iterator alloc_itr;

#define MAX_ENTRIES 100*1024*1024
unsigned int *allocmap_arr=NULL;
unsigned int *alloc_prot_map_arr=NULL;
static int init_alloc;
static unsigned int alloc_cnt;

#ifdef _NVRAM_OPTIMIZE
#define CACHE_ENTRIES 8
UINT vmaid_cache[CACHE_ENTRIES];
UINT vmasize_cache[CACHE_ENTRIES];
UINT next_vma_entry;
#endif

struct entry_s {
	char *key;
	unsigned int value;
	struct entry_s *next;
};
 
typedef struct entry_s entry_t;
 
struct hashtable_s {
	int size;
	struct entry_s **table;	
};
 
typedef struct hashtable_s hashtable_t;
unsigned long *hash_cache = NULL;
 
 
/* Create a new hashtable. */
hashtable_t *ht_create( int size ) {
 
	hashtable_t *hashtable = NULL;
	int i;
 
	if( size < 1 ) return NULL;
 
	/* Allocate the table itself. */
	if( ( hashtable = (hashtable_t *)malloc( 
			sizeof( hashtable_t ) ) ) == NULL ) {
		return NULL;
	}
 
	/* Allocate pointers to the head nodes. */
	if( ( hashtable->table = (entry_t **)malloc( 
			sizeof( entry_t * ) * size ) ) == NULL ) {
		return NULL;
	}
	for( i = 0; i < size; i++ ) {
		hashtable->table[i] = NULL;
	}
 
	hashtable->size = size;
 
	return hashtable;	
}
 
/* Hash a string for a particular hash table. */
int ht_hash( hashtable_t *hashtable, char *key ) {
 
	unsigned long int hashval;
	int i = 0;
 
	/* Convert our string to an integer */
	while( hashval < ULONG_MAX && i < strlen( key ) ) {
		hashval = hashval << 8;
		hashval += key[ i ];
		i++;
	}
 
	return hashval % hashtable->size;
}
 
/* Create a key-value pair. */
entry_t *ht_newpair( char *key, unsigned int value ) {
	entry_t *newpair;
 
	if( ( newpair = (entry_t *)malloc( sizeof( entry_t ) ) ) == NULL ) {
		return NULL;
	}
	if( ( newpair->key = strdup( key ) ) == NULL ) {
		return NULL;
	}

	newpair->value = value;
	newpair->next = NULL;
	return newpair;
}
 
/* Insert a key-value pair into a hash table. */
void ht_set( hashtable_t *hashtable, char *key, unsigned int value ) {
	int bin = 0;
	entry_t *newpair = NULL;
	entry_t *next = NULL;
	entry_t *last = NULL;
 
	bin = ht_hash( hashtable, key );
	next = hashtable->table[ bin ];
	hash_cache[alloc_cnt] = bin;
 
#if 1
	while( next != NULL && next->key != NULL && strcmp( key, next->key ) > 0 ) {
		last = next;
		next = next->next;
	}
 
	/* There's already a pair.  Let's replace that string. */
	if( next != NULL && next->key != NULL && strcmp( key, next->key ) == 0 ) {
 
		//free( next->value );
		//next->value = strdup( value );
 		next->value = value;

	/* Nope, could't find it.  Time to grow a pair. */
	} else {
		newpair = ht_newpair( key, value );
 
		/* We're at the start of the linked list in this bin. */
		if( next == hashtable->table[ bin ] ) {
			newpair->next = next;
			hashtable->table[ bin ] = newpair;
	
		/* We're at the end of the linked list in this bin. */
		} else if ( next == NULL ) {
			last->next = newpair;
	
		/* We're in the middle of the list. */
		} else  {
			newpair->next = next;
			last->next = newpair;
		}
	}
#endif
}
 
/* Retrieve a key-value pair from a hash table. */
unsigned int ht_get( hashtable_t *hashtable, char *key ) {
	int bin = 0;
	entry_t *pair;
 
	bin = ht_hash( hashtable, key );
 
	/* Step through the bin, looking for our value. */
	pair = hashtable->table[ bin ];
	while( pair != NULL && pair->key != NULL && strcmp( key, pair->key ) > 0 ) {
		pair = pair->next;
	}
 
	/* Did we actually find anything? */
	if( pair == NULL || pair->key == NULL || strcmp( key, pair->key ) != 0 ) {
		return 0;
 
	} else {
		return pair->value;
	}
	
}

hashtable_t *hashtable = NULL;
unsigned long *allocmap_long; 
unsigned long *allocmap_sz; 


void add_alloc_map(void* ptr, size_t size){

	unsigned long addr = (unsigned long)ptr;
	allocmap[addr] = size;

	/*std::stringstream strstream;
	std::string str;
	strstream << addr;
	char *key = strstream.str().c_str();
	//fprintf(stdout,"ptr %lu\n",(unsigned long)addr);*/

	if(!init_alloc){
	    //hashtable = ht_create(MAX_ENTRIES);
		//hash_cache = (unsigned long *)malloc(sizeof(unsigned long) * MAX_ENTRIES);
		allocmap_long = (unsigned long *)malloc(sizeof(unsigned long) * MAX_ENTRIES);
		allocmap_sz = (unsigned long *)malloc(sizeof(unsigned long) * MAX_ENTRIES);
		init_alloc = 1;
	}
	allocmap_long[alloc_cnt]= addr;
	allocmap_sz[alloc_cnt]= size;
	alloc_cnt++;

	//fprintf(stdout,"%s \n",key);
	/*ht_set( hashtable, key, size );
   	 alloc_cnt++;*/
	//alloc_prot_map[ptr] = 0;
}

#if 1
size_t get_alloc_size(void *ptr, unsigned long *faddr){

	int idx = 0;
	unsigned int value=0;
	unsigned long addr=0, endaddr=0;

	/*if(allocmap.find((void *)ptr) != allocmap.end()){
		*faddr = ptr;
		return allocmap[ptr];
	}*/
   for( alloc_itr= allocmap.begin(); alloc_itr!=allocmap.end(); ++alloc_itr){
        addr = (unsigned long)(*alloc_itr).first;
		endaddr = (unsigned long) (addr + (size_t)(*alloc_itr).second);
		if((unsigned long)ptr >= addr && (unsigned long)ptr <= endaddr) {
			*faddr = (unsigned long)addr;
			return (size_t)(*alloc_itr).second;
		} 
	}
	return 0;

	for(idx = 0; idx < alloc_cnt; idx++){

		addr = allocmap_long[idx];
		value = allocmap_sz[idx];
		endaddr = addr + value;
		if((unsigned long)ptr >= addr && (unsigned long)ptr <= endaddr) {
			*faddr = addr;
			 //fprintf(stdout,"addr %lu %u %u\n",addr, value, alloc_cnt);
			 return value;
		}
	}
	return 0;	
}
#endif



#if 0
size_t get_alloc_size(void *ptr, unsigned long *faddr){

	int idx = 0;
	int bin = 0;
	entry_t *pair;
	unsigned int value=0;
	unsigned long addr=0, endaddr=0;

	/*unsigned long laddr = (unsigned long)ptr;
	std::stringstream strstream;
	std::string str;
	strstream << laddr;
	char *key = strstream.str().c_str();*/

	/*value = ht_get( hashtable, key );
	if(value) {
		endaddr = addr + value;
		if(ptr >= addr && ptr <= endaddr) {
    		 *faddr = addr;
		      return value;
		 }
	}*/

	for(idx = 0; idx < alloc_cnt; idx++){

		bin = hash_cache[idx];
		pair = hashtable->table[ bin ];
		if(!pair) continue;	

		assert(pair->key);


		value = ht_get( hashtable, pair->key );
		if(!value) continue;

		/*addr = strtoul (pair->key, NULL, 0);
		assert(addr);

		if(ptr < addr) continue;	



	
		endaddr = addr + value;
		if(ptr >= addr && ptr <= endaddr) {
			*faddr = addr;
			 //fprintf(stdout,"addr %lu %u %u\n",addr, value, alloc_cnt);
			 return value;
		}*/
	}
	return 0;	
}

#endif

/*void protect_all_chunks(){ 

	std::unordered_map <void *, size_t>::iterator itr;

	for( itr= allocmap.begin(); itr!=allocmap.end(); ++itr){
        void *addr = (void *)(*itr).first;
		set_chunk_protection(addr,(size_t)(*itr).second,PROT_READ);
		fprintf(stderr,"setting protection for all chunks\n");
	}
}*/

#if 0
size_t get_alloc_size(void *ptr, unsigned long *faddr){


	if(allocmap.find((void *)ptr) != allocmap.end()){
		*faddr = ptr;
		return allocmap[ptr];
	}
   for( alloc_itr= allocmap.begin(); alloc_itr!=allocmap.end(); ++alloc_itr){
        void *addr = (void *)(*alloc_itr).first;
		void *endaddr = addr + (size_t)(*alloc_itr).second;
		//printf("fail %lu\n",(unsigned long)addr);
		if(ptr >= addr && ptr <= endaddr) {
			//fprintf(stderr,"success %u \n",(size_t)(*alloc_itr).second);
			*faddr = (unsigned long)addr;
			return (size_t)(*alloc_itr).second;
		} 
	}
	//fprintf(stderr,"failed \n");
	return 0;	
}
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


