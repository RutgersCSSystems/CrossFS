/* -*- C++ -*- */

/*
  The Hoard Multiprocessor Memory Allocator
  www.hoard.org

  Author: Emery Berger, http://www.cs.umass.edu/~emery
 
  Copyright (c) 1998-2012 Emery Berger

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/*
 * @file   libhoard.cpp
 * @brief  This file replaces malloc etc. in your application.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */
#define _POSIX_C_SOURCE 200809L

#include "heaplayers.h"
using namespace HL;

#include <new>
//#include "hash_maps.h"
#include <numa.h>
#include <time.h>
#include <inttypes.h>
#include "heaplayers/wrappers/mmapwrapper.h"
#include <migration.h>


pthread_mutex_t mutex;

#define MAXPAGELISTSZ 1024*1024*100

// The undef below ensures that any pthread_* calls get strong
// linkage.  Otherwise, our versions here won't replace them.  It is
// IMPERATIVE that this line appear before any files get included.

#undef __GXX_WEAK__ 

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN

// Maximize the degree of inlining.
#pragma inline_depth(255)

// Turn inlining hints into requirements.
#define inline __forceinline
#pragma warning(disable:4273)
#pragma warning(disable: 4098)  // Library conflict.
#pragma warning(disable: 4355)  // 'this' used in base member initializer list.
#pragma warning(disable: 4074)	// initializers put in compiler reserved area.
#pragma warning(disable: 6326)  // comparison between constants.

#endif

#if HOARD_NO_LOCK_OPT
// Disable lock optimization.
volatile bool anyThreadCreated = true;
#else
// The normal case. See heaplayers/spinlock.h.
volatile bool anyThreadCreated = false;
#endif

namespace Hoard {
  
  // HOARD_MMAP_PROTECTION_MASK defines the protection flags used for
  // freshly-allocated memory. The default case is that heap memory is
  // NOT executable, thus preventing the class of attacks that inject
  // executable code on the heap.
  // 
  // While this is not recommended, you can define HL_EXECUTABLE_HEAP as
  // 1 in heaplayers/heaplayers.h if you really need to (i.e., you're
  // doing dynamic code generation into malloc'd space).
  
#if HL_EXECUTABLE_HEAP
#define HOARD_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define HOARD_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE)
#endif

}

#include "hoardtlab.h"

//
// The base Hoard heap.
//


using namespace Hoard;

/// Maintain a single instance of the main Hoard heap.

HoardHeapType * getMainHoardHeap (void) {
  // This function is C++ magic that ensures that the heap is
  // initialized before its first use. First, allocate a static buffer
  // to hold the heap.

  static double thBuf[sizeof(HoardHeapType) / sizeof(double) + 1];

  // Now initialize the heap into that buffer.
  static HoardHeapType * th = new (thBuf) HoardHeapType;
  return th;
}

TheCustomHeapType * getCustomHeap();

extern "C" {
 
  static size_t xxactalloc;
  static size_t xxtotalloc;
 
  void addallocsz(size_t sz){	 
    xxactalloc += sz;
    xxtotalloc += sz;
  }	

  void suballocsz(size_t sz){	 
    xxactalloc -= sz;
  }		 	
  void printallocsz(size_t sz){
    fprintf(stderr,"active alloc size %zu, tot alloc %zu\n",xxactalloc, xxtotalloc);	
  }


#if 0
 void *get_page_list(unsigned int *pgcount) {
	
	//void **addr = NULL;
	addr = MmapWrapper::get_app_page_list(pgcount);
	assert(addr);
	//printf("sendin addr %lu, pagecount %u \n",
	//		(unsigned long)addr,*pgcount);
	return addr;
 }
#endif

struct timespec spec;
unsigned int migrate_off;
	 	 

void * xxmalloc (size_t sz) {
  TheCustomHeapType * h = getCustomHeap();
  void * ptr = h->malloc (sz);
#ifdef _DEBUG
  addallocsz(sz);
  printallocsz(sz);
#endif
  //fprintf(stdout,"recording address \n");
  //record_addr(ptr,sz);
  return ptr;
}

void xxfree (void * ptr) {
#ifdef _DEBUG
   if(ptr)
     suballocsz(getCustomHeap()->getSize (ptr));	
#endif
    getCustomHeap()->free (ptr);
  }


#if 0
//int init = 0;
struct bitmask *old_nodes;
struct bitmask *new_nodes;

#if 1

void **get_pages(unsigned long *alloc_arr, size_t *sizearr, 
				unsigned int alloc_cnts, void** migpagelist,
				unsigned int *migcnt){

	int cnt=0, i=0, page_count=0;
	long pagesize=4096;
	char *pages=NULL;
	void *page_base = NULL;
	size_t allocsz=0;
	long offset = 0;
	
	for(cnt =0; cnt < alloc_cnts; cnt++){

		allocsz = sizearr[cnt];
		page_count = allocsz/pagesize; 
		page_base = (void *)alloc_arr[cnt];

		pages = (char *) ((((long)page_base) & ~((long)(pagesize - 1))) + pagesize);

		//printf("page_count %u, offset %u\n",page_count, offset);
		for (i = 0; i < page_count; i++) {
			  migpagelist[offset] = pages + i * pagesize;
			  offset++;
		}
	}
	*migcnt = offset;

	return migpagelist;
}


void migrate_pages(int node) {

	unsigned int pgcount=0, migcnt=0;
	int *status;
	int *nodes;
	int i =0, rc=0;
	struct timespec currspec;
	int sec;
	int nr_nodes = 2; //numa_max_node()+1;
	void **migpagelist = NULL;
	size_t *sizearr = NULL;
#if 0
	clock_gettime(CLOCK_REALTIME, &currspec);

	 sec = currspec.tv_sec - spec.tv_sec;

     if((currspec.tv_sec - spec.tv_sec) < 1)
        goto gettime;

     if(!init) {
        //numa_init();
        nr_nodes = numa_max_node()+1;
        //printf("print migrate pages %d\n",nr_nodes);
        old_nodes = numa_bitmask_alloc(nr_nodes);
        new_nodes = numa_bitmask_alloc(nr_nodes);
        numa_bitmask_setbit(old_nodes, 0);
        numa_bitmask_setbit(new_nodes, 1);
        init = 1;
    }
	//printf("Migrating pages\n");
	rc = numa_migrate_pages(0, old_nodes, new_nodes);
    if (rc < 0) {
        perror("numa_migrate_pages failed");
		exit(-1);
    }
	//printf("time diff sec %d \n", sec);
	clock_gettime(CLOCK_REALTIME, &spec);
gettime:
	return;
#endif

#if 1
	void *memptr=NULL;

	clock_gettime(CLOCK_REALTIME, &currspec);

	if((currspec.tv_sec - spec.tv_sec) < 10)
		goto gettime;

	memptr = get_alloc_pagemap(&pgcount, &sizearr);
	assert(pgcount);

	pthread_mutex_lock(&mutex);

	migpagelist = (void **)mmap (0, MAXPAGELISTSZ, 
								PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	get_pages((unsigned long*)memptr, sizearr, pgcount, migpagelist,&migcnt);
	printf("migcnt %d \n",migcnt);

	pgcount = migcnt;
	status =(int *)mmap (0, sizeof(int) * pgcount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	nodes =	(int *)mmap (0, sizeof(int) * pgcount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	assert(migpagelist);
	assert(status);
	assert(nodes);

	node = 1;
	for (i = 0; i < pgcount; i++) {
         nodes[i] = node;
         status[i] = -123;
     }



	//fprintf(stdout,"migrate_pages(), pagecount %u\n", pgcount);

	 /* Move to node zero */
	numa_move_pages(0, migcnt, migpagelist, nodes, status, 0);
	//fprintf(stdout,"migrated pages %u \n", migrate_off);
	clock_gettime(CLOCK_REALTIME, &spec);
	migrate_off += pgcount;

	munmap(status,sizeof(int) * pgcount);
	munmap(nodes,sizeof(int) * pgcount);
	munmap(migpagelist,MAXPAGELISTSZ);

	pthread_mutex_unlock(&mutex);
gettime:
	;
#endif



#if 0
	clock_gettime(CLOCK_REALTIME, &currspec);

	if((currspec.tv_sec - spec.tv_sec) < 1)
		goto gettime;

	addr = MmapWrapper::get_app_page_list(&pgcount);
	assert(pgcount);

	pthread_mutex_lock(&mutex);

	status =(int *)mmap (0, sizeof(int) * pgcount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	nodes =	(int *)mmap (0, sizeof(int) * pgcount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(status);
	assert(nodes);
	for (i = 0; i < pgcount; i++) {
            nodes[i] = node;
            status[i] = -123;
     }

	//fprintf(stdout,"migrate_off %u pgcount %u\n", migrate_off,pgcount);

	 /* Move to node zero */
	numa_move_pages(0, pgcount, addr, nodes, status, 0);
	//fprintf(stdout,"migrated pages %u \n", migrate_off);
	clock_gettime(CLOCK_REALTIME, &spec);
	migrate_off += pgcount;
	munmap(status,sizeof(int) * pgcount);
	munmap(nodes,sizeof(int) * pgcount);
	pthread_mutex_unlock(&mutex);

gettime:
	;
#endif
}
#endif
#endif

  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize (ptr);
  }

  void xxmalloc_lock() {
    // Undefined for Hoard.
  }

  void xxmalloc_unlock() {
    // Undefined for Hoard.
  }

}
