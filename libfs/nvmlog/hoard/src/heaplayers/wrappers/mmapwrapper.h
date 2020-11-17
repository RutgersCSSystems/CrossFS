// -*- C++ -*-

/*

  Heap Layers: An Extensible Memory Allocation Infrastructure
  
  Copyright (C) 2000-2012 by Emery Berger
  http://www.cs.umass.edu/~emery
  emery@cs.umass.edu
  
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

#ifndef HL_MMAPWRAPPER_H
#define HL_MMAPWRAPPER_H

#if defined(_WIN32)
#include <windows.h>
#else
// UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <map>
#include <assert.h>
#endif
#include <numa.h>

#if HL_EXECUTABLE_HEAP
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define HL_MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE)
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif


//#define SINGLE_LARGE_MMAP
#ifdef SINGLE_LARGE_MMAP
#define MAX 1024*1024*1024*1
static void *g_ptr=NULL;
static void *g_currptr=NULL;
#endif

//#define _USE_NVMAP
#ifdef _USE_NVMAP
#include <time.h> 
static unsigned int BASEPROCID=0;
#define __NR_nv_mmap_pgoff     314
#define _USERANDOM_PROCID
struct nvmap_arg_struct {

    unsigned long fd;
    unsigned long offset;
    int vma_id;
    int proc_id;
    /*flags related to persistent memory usage*/
    int pflags;
    int noPersist; // indicates if this mmapobj is persistent or not
    int ref_count;
};
typedef struct nvmap_arg_struct nvarg_s;

static size_t nvm_tot_allocs;
static void nvm_stat_print();
static void nvm_add_alloc_stat(size_t sz);
static void nvm_add_free_stat(size_t sz);

static void nvm_add_alloc_stat(size_t sz){
	nvm_tot_allocs += sz;
}
static void nvm_add_free_stat(size_t sz){
	nvm_tot_allocs -= sz;
}

static void nvm_stat_print(){
	fprintf(stderr,"total active allocs %zu \n",nvm_tot_allocs);
}
#endif

static unsigned int pagesize;
static unsigned int page_count, numpages;
static size_t mapsize;
static int offset;
static void **addr;
static int init;

namespace HL {

  class MmapWrapper {
  public:

#if defined(_WIN32) 
  
    // Microsoft Windows has 4K pages aligned to a 64K boundary.
    enum { Size = 4 * 1024 };
    enum { Alignment = 64 * 1024 };

#elif defined(__SVR4)
    // Solaris aligns 8K pages to a 64K boundary.
    enum { Size = 8 * 1024 };
    enum { Alignment = 64 * 1024 };

#else
    // Linux and most other operating systems align memory to a 4K boundary.
    enum { Size = 4 * 1024 };
    enum { Alignment = 4 * 1024 };

#endif
    // Release the given range of memory to the OS (without unmapping it).
    void release (void * ptr, size_t sz) {
      if ((size_t) ptr % Alignment == 0) {
	// Extra sanity check in case the superheap's declared alignment is wrong!
#if defined(_WIN32)
	VirtualAlloc (ptr, sz, MEM_RESET, PAGE_NOACCESS);
#elif defined(__APPLE__)
	madvise (ptr, sz, MADV_DONTNEED);
	madvise (ptr, sz, MADV_FREE);
#else
	// Assume Unix platform.
	madvise ((caddr_t) ptr, sz, MADV_DONTNEED);
#endif
      }
    }

#if defined(_WIN32) 
  
    static void * map (size_t sz) {
      void * ptr;
#if HL_EXECUTABLE_HEAP
      const int permflags = PAGE_EXECUTE_READWRITE;
#else
      const int permflags = PAGE_READWRITE;
#endif
      ptr = VirtualAlloc (NULL, sz, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, permflags);
      return  ptr;
    }
  
    static void unmap (void * ptr, size_t) {
      VirtualFree (ptr, 0, MEM_RELEASE);
    }

#else


static void clear_migrated_pages(int *status, unsigned int pgcount) {

	void **tmp = MmapWrapper::create_addr_buff();
	unsigned int cnt=0;
	
	offset = 0;

	for(cnt=0; cnt < pgcount; cnt++){	

		/*if pages were not migrated 
		* then retry */
		if(status[cnt] != 1) {
			tmp[offset] = addr[cnt];
			offset++;
		}
	}	

	MmapWrapper::del_addr_buff();

	addr = tmp;

	return;
}



static void migrate_pages(int node) {

    unsigned int pgcount=0;
    int *status;
    int *nodes;
    int i =0, rc=0;
	void **tmp =NULL;
	struct bitmask *old_nodes;
	struct bitmask *new_nodes;
	
	 int nr_nodes = 2; //numa_max_node()+1;

	 if(!init) {
		//numa_init();
		nr_nodes = numa_max_node()+1;
		fprintf(stderr,"print migrate pages %d\n",nr_nodes);
		//old_nodes = numa_bitmask_alloc(nr_nodes);
		//new_nodes = numa_bitmask_alloc(nr_nodes);
		//old_nodes = numa_allocate_cpumask();
    	//new_nodes = numa_allocate_cpumask();
     	//numa_bitmask_setbit(old_nodes, 0);
     	//numa_bitmask_setbit(new_nodes, 1);
	    init = 1;
	}

	fprintf(stderr,"migrate pages \n");
	
	//numa_migrate_pages(0, old_nodes, new_nodes);
	//rc = numa_migrate_pages(0, old_nodes, new_nodes);
    if (rc < 0) {
    	perror("numa_migrate_pages failed");
    }

	return;

    //struct timespec currspec;
    //clock_gettime(CLOCK_REALTIME, &currspec);
    //if((currspec.tv_sec - spec.tv_sec) < 1)
      //  goto gettime;

    addr = MmapWrapper::get_app_page_list(&pgcount);
    assert(pgcount);

    //pthread_mutex_lock(&mutex);
    status =(int *)mmap (0, sizeof(int) * pgcount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    nodes = (int *)mmap (0, sizeof(int) * pgcount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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
    //clock_gettime(CLOCK_REALTIME, &spec);
 	munmap(status,sizeof(int) * pgcount);
    munmap(nodes,sizeof(int) * pgcount);
    //pthread_mutex_unlock(&mutex);
gettime:
    ;
}



static void** get_app_page_list(unsigned int *size){

	*size = offset;
	 void **ptr = addr;
	 assert(ptr);
     //printf("sendin addr %lu, pagecount %u \n",
     //       ptr,*size);

	/*reinitialize offset=0, pages attempted
	* for migration */
	offset=0;

	return ptr; 
}


static unsigned int getmapsize(){
	mapsize = 1024*1024*512;
	return mapsize;
}	

static void **create_addr_buff() {

	void **tmp = NULL;
	
	 getmapsize();

	offset=0;
   	tmp = (void **)mmap (0, mapsize, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tmp == NULL || tmp == MAP_FAILED) {
   	    perror("Unable to allocate memory\n");
        exit(1);
	}
	return tmp;
}

static void del_addr_buff(){
	munmap(addr, mapsize);
	addr = NULL;
}


//int main(int argc, char **argv)
static void add_user_pages(void *page_base, size_t size) 
{
      int i, rc;
	  char *pages;

	 getmapsize();

	 pagesize = 4096; //getpagesize();
	 page_count = size/pagesize;
	

	 if(!addr) addr = create_addr_buff();

	 pages = (char *) ((((long)page_base) & ~((long)(pagesize - 1))) + pagesize);

	if((offset * sizeof(void*) * sizeof(void*) ) > mapsize){

		/*time to migrate*/
		MmapWrapper::migrate_pages(1);

		offset = 0;

		/*delete exisiting location*/
		del_addr_buff();
		
		/*create new addr buff */
		addr = create_addr_buff();
	}

	//printf("page_count %u, offset %u\n",page_count, offset);
	for (i = 0; i < page_count; i++) {
      addr[offset] = pages + i * pagesize;
	  offset++;
	}
	numpages = offset;

	//fprintf(stdout,"numpages %u offset %u \n",numpages, offset);
	return; 
}	



static void * map (size_t sz) {

      if (sz == 0) {
		return NULL;
      }

      // Round up the size to a page-sized value.
      sz = Size * ((sz + Size - 1) / Size);

      void * ptr;

#if defined(MAP_ALIGN) && defined(MAP_ANON)
      // Request memory aligned to the Alignment value above.
      ptr = mmap ((char *) Alignment, sz, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ALIGN | MAP_ANON, -1, 0);
      fprintf(stdout,"mmap 3\n");
#elif !defined(MAP_ANONYMOUS)
      static int fd = ::open ("/dev/zero", O_RDWR);
      ptr = mmap (NULL, sz, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE, fd, 0);
      fprintf(stdout,"mmap 2\n");	
#else

 
#ifndef _USE_NVMAP
 #ifdef SINGLE_LARGE_MMAP
     if(!g_ptr || ((g_currptr+sz) > g_ptr + MAX)) {
		 fprintf(stdout,"mmaping \n");
	     g_ptr = mmap (0,MAX, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		 assert(g_ptr != MAP_FAILED);
		 assert(g_ptr != NULL);
		 g_currptr = g_ptr;
	}
	ptr = g_currptr;
	g_currptr += sz;
	
 #else
	  //fprintf(stdout,"calling mmap HL_MMAP_PROTECTION_MASK \n");	
      ptr = mmap (0, sz, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	  //MmapWrapper::add_user_pages(ptr,sz);	
      //MmapWrapper::migrate_pages(1);	
 #endif // #ifndef _USE_NVMAP

#else //_USE_NVMAP
	if(!BASEPROCID){
#ifdef _USERANDOM_PROCID
      struct timeval currtime;
	  int iSecret;
      /* initialize random seed: */
	  srand (time(NULL));
	  /* generate secret number between 1 and 10: */
      gettimeofday(&currtime, NULL);
	  iSecret = (rand() % currtime.tv_usec % 533333 ) + 1;
      BASEPROCID= iSecret;
#else
      BASEPROCID = 600;
#endif
	  printf("BASEPROCID %u\n",BASEPROCID);
	}
	 nvarg_s nvarg;
	 nvarg.pflags = 1;
	 nvarg.noPersist = 1;
	 nvarg.vma_id = 9998;
	 nvarg.proc_id = BASEPROCID;


#ifdef SINGLE_LARGE_MMAP
     if(!g_ptr || ((g_currptr+sz) > g_ptr + MAX)) {
		 fprintf(stdout,"mmaping \n");
		 g_ptr = (void *)syscall(__NR_nv_mmap_pgoff,0, MAX, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, &nvarg);
		 assert(g_ptr != MAP_FAILED);
		 assert(g_ptr != NULL);
		 g_currptr = g_ptr;
		 unsigned int i=0;
		 char *temp = (char *)g_currptr;
		 char a;
		 while(i<MAX){	
			temp[i]++;
			temp[i]=0;
			i++;
		}	
		fprintf(stdout,"finished mapping \n");
	}	
	ptr = g_currptr;
	g_currptr += sz;

#else
	 //nvm_add_alloc_stat(sz);
	 fprintf(stdout,"using NVMMAP %u\n", sz);
	 ptr = (void *)syscall(__NR_nv_mmap_pgoff,0, sz, HL_MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, &nvarg);
	 //nvm_stat_print(); 
#endif
     //np_nvmap  = (char *)syscall(__NR_nv_mmap_pgoff,0 ,s,  PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, &a);
#endif
      //fprintf(stdout,"mmap 1\n");	
#endif

      if (ptr == MAP_FAILED) {
	char buf[255];
	sprintf (buf, "Out of memory!");
	fprintf (stderr, "%s\n", buf);
	return NULL;
      } else {
	return ptr;
      }
    }

    static void unmap (void * ptr, size_t sz) {
      // Round up the size to a page-sized value.
      sz = Size * ((sz + Size - 1) / Size);
      munmap ((caddr_t) ptr, sz);
    }
   
#endif

  };

}

#endif
