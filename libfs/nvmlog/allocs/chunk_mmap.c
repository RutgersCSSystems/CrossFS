#define	JEMALLOC_CHUNK_MMAP_C_
#include "jemalloc/internal/jemalloc_internal.h"

#ifdef _USENVRAM
#include "nv_map.h"

extern int proc_persist_id;
struct nvmap_arg_struct a;
unsigned int chunk_id = 1000;
unsigned int proc_id = 4000;
#endif

/******************************************************************************/
/* Function prototypes for non-inline static functions. */

static void	*pages_map(void *addr, size_t size);
static void	pages_unmap(void *addr, size_t size);
static void	*chunk_alloc_mmap_slow(size_t size, size_t alignment,
    bool *zero);

/******************************************************************************/

static void *
pages_map(void *addr, size_t size)
{
	void *ret;

	assert(size != 0);

#ifdef _WIN32
	/*
	 * If VirtualAlloc can't allocate at the given address when one is
	 * given, it fails and returns NULL.
	 */
	ret = VirtualAlloc(addr, size, MEM_COMMIT | MEM_RESERVE,
	    PAGE_READWRITE);
#else
	/*
	 * We don't use MAP_FIXED here, because it can cause the *replacement*
	 * of existing mappings, and we only want to create new mappings.
	 */
#ifdef _USENVRAM
	nvarg_s a;

   a.proc_id = proc_persist_id;	

	ret = _mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
	  -1, 0, &a);
#else
	ret = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
	  -1, 0);
#endif

	assert(ret != NULL);

	if (ret == MAP_FAILED)
		ret = NULL;
	else if (addr != NULL && ret != addr) {
		/*
		 * We succeeded in mapping memory, but not in the right place.
		 */
		/*if (munmap(ret, size) == -1) {
			char buf[BUFERROR_BUF];

			buferror(buf, sizeof(buf));
			malloc_printf("<jemalloc: Error in munmap(): %s\n",
			    buf);
			if (opt_abort)
				abort();
		}*/
		ret = NULL;
	}
#endif
	assert(ret == NULL || (addr == NULL && ret != addr)
	    || (addr != NULL && ret == addr));
	return (ret);
}


static void *
nv_pages_map(void *addr, size_t size, unsigned long *strt_addr)

{
	void *ret;

	assert(size != 0);

	//fprintf(stderr,"calling pages_map \n");

#ifdef _WIN32
	/*
	 * If VirtualAlloc can't allocate at the given address when one is
	 * given, it fails and returns NULL.
	 */
	ret = VirtualAlloc(addr, size, MEM_COMMIT | MEM_RESERVE,
	    PAGE_READWRITE);
#else
	/*
	 * We don't use MAP_FIXED here, because it can cause the *replacement*
	 * of existing mappings, and we only want to create new mappings.
	 */
	ret = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
	  -1, 0);
	*strt_addr = (unsigned long)ret;

	/*a.fd = -1;
    a.chunk_id = chunk_id++;
    a.proc_id = proc_id;
    a.pflags = 1;
    a.noPersist = 1;
	ret = (char *)syscall(__NR_nv_mmap_pgoff,addr,size,  PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, &a);*/
	//fprintf(stderr, "chunk_id %d  start: %lu end: %lu\n", chunk_id, ret, ret+size);

	assert(ret != NULL);

	if (ret == MAP_FAILED)
		ret = NULL;
	else if (addr != NULL && ret != addr) {
		/*
		 * We succeeded in mapping memory, but not in the right place.
		 */
		/*if (munmap(ret, size) == -1) {
			char buf[BUFERROR_BUF];

			buferror(buf, sizeof(buf));
			malloc_printf("<jemalloc: Error in munmap(): %s\n",
			    buf);
			if (opt_abort)
				abort();
		}*/
		ret = NULL;
	}
#endif
	assert(ret == NULL || (addr == NULL && ret != addr)
	    || (addr != NULL && ret == addr));
	return (ret);
}



static void
pages_unmap(void *addr, size_t size)
{

#ifdef _WIN32
	if (VirtualFree(addr, 0, MEM_RELEASE) == 0)
#else
	//if (munmap(addr, size) == -1)
#endif
	/*{
		char buf[BUFERROR_BUF];

		buferror(buf, sizeof(buf));
		malloc_printf("<jemalloc>: Error in "
#ifdef _WIN32
		              "VirtualFree"
#else
		              "munmap"
#endif
		              "(): %s\n", buf);
		if (opt_abort)
			abort();
	}*/
}

static void *
pages_trim(void *addr, size_t alloc_size, size_t leadsize, size_t size)
{
	void *ret = (void *)((uintptr_t)addr + leadsize);

	assert(alloc_size >= leadsize + size);
#ifdef _WIN32
	{
		void *new_addr;

		pages_unmap(addr, alloc_size);
		new_addr = pages_map(ret, size);
		if (new_addr == ret)
			return (ret);
		if (new_addr)
			pages_unmap(new_addr, size);
		return (NULL);
	}
#else
	{
		size_t trailsize = alloc_size - leadsize - size;

		if (leadsize != 0)
			pages_unmap(addr, leadsize);
		if (trailsize != 0)
			pages_unmap((void *)((uintptr_t)ret + size), trailsize);
		return (ret);
	}
#endif
}

void
pages_purge(void *addr, size_t length)
{

#ifdef _WIN32
	VirtualAlloc(addr, length, MEM_RESET, PAGE_READWRITE);
#else
#  ifdef JEMALLOC_PURGE_MADVISE_DONTNEED
#    define JEMALLOC_MADV_PURGE MADV_DONTNEED
#  elif defined(JEMALLOC_PURGE_MADVISE_FREE)
#    define JEMALLOC_MADV_PURGE MADV_FREE
#  else
#    error "No method defined for purging unused dirty pages."
#  endif
	madvise(addr, length, JEMALLOC_MADV_PURGE);
#endif
}


#ifdef _USENVRAM
static void *
nv_chunk_alloc_mmap_slow(size_t size, size_t alignment, bool *zero,
						unsigned long addr)

{
	void *ret, *pages;
	size_t alloc_size, leadsize;

	alloc_size = size + alignment - PAGE;
	/* Beware size_t wrap-around. */
	if (alloc_size < size)
		return (NULL);
	do {
		pages = (void *)nv_pages_map(NULL, alloc_size, &addr);
		if (pages == NULL)
			return (NULL);
		leadsize = ALIGNMENT_CEILING((uintptr_t)pages, alignment) -
		    (uintptr_t)pages;
		ret = pages_trim(pages, alloc_size, leadsize, size);
	} while (ret == NULL);

	assert(ret != NULL);
	*zero = true;
	return (ret);
}

#endif

static void *
chunk_alloc_mmap_slow(size_t size, size_t alignment, bool *zero)

{
	void *ret, *pages;
	size_t alloc_size, leadsize;

	alloc_size = size + alignment - PAGE;
	/* Beware size_t wrap-around. */
	if (alloc_size < size)
		return (NULL);
	do {
		pages = pages_map(NULL, alloc_size);
		if (pages == NULL)
			return (NULL);
		leadsize = ALIGNMENT_CEILING((uintptr_t)pages, alignment) -
		    (uintptr_t)pages;
		ret = pages_trim(pages, alloc_size, leadsize, size);
	} while (ret == NULL);

	assert(ret != NULL);
	*zero = true;
	return (ret);
}


void *
chunk_alloc_mmap(size_t size, size_t alignment, bool *zero)
{
	void *ret;
	size_t offset;


	/*
	 * Ideally, there would be a way to specify alignment to mmap() (like
	 * NetBSD has), but in the absence of such a feature, we have to work
	 * hard to efficiently create aligned mappings.  The reliable, but
	 * slow method is to create a mapping that is over-sized, then trim the
	 * excess.  However, that always results in one or two calls to
	 * pages_unmap().
	 *
	 * Optimistically try mapping precisely the right amount before falling
	 * back to the slow method, with the expectation that the optimistic
	 * approach works most of the time.
	 */

	assert(alignment != 0);
	assert((alignment & chunksize_mask) == 0);

	//fprintf(stderr,"pages_map %u\n",size);
	ret = pages_map(NULL, size);

	if (ret == NULL)
		return (NULL);
	offset = ALIGNMENT_ADDR2OFFSET(ret, alignment);
	if (offset != 0) {
		pages_unmap(ret, size);
		return (chunk_alloc_mmap_slow(size, alignment, zero));
	}

	assert(ret != NULL);
	*zero = true;
	return (ret);
}



#ifdef _USENVRAM
void *
nv_chunk_alloc_mmap(size_t size, size_t alignment, bool *zero, void *rqst)
{
	void *ret;
	size_t offset;
    struct rqst_struct *rq_data;    
	unsigned long addr = 0;
	rq_data = ( struct rqst_struct *)rqst;


	/*
	 * Ideally, there would be a way to specify alignment to mmap() (like
	 * NetBSD has), but in the absence of such a feature, we have to work
	 * hard to efficiently create aligned mappings.  The reliable, but
	 * slow method is to create a mapping that is over-sized, then trim the
	 * excess.  However, that always results in one or two calls to
	 * pages_unmap().
	 *
	 * Optimistically try mapping precisely the right amount before falling
	 * back to the slow method, with the expectation that the optimistic
	 * approach works most of the time.
	 */

	assert(alignment != 0);
	assert((alignment & chunksize_mask) == 0);

	ret = nv_pages_map(NULL,size, &rq_data->mmapobj_straddr);

	if (ret == NULL)
		return (NULL);
	offset = ALIGNMENT_ADDR2OFFSET(ret, alignment);
	if (offset != 0) {
		pages_unmap(ret, size);
		addr = (unsigned long)rq_data->mmapobj_straddr;
		return (nv_chunk_alloc_mmap_slow(size, alignment, zero, addr));
	}

	assert(ret != NULL);
	*zero = true;
	return (ret);
}

#endif

bool
chunk_dealloc_mmap(void *chunk, size_t size)
{

	if (config_munmap)
		pages_unmap(chunk, size);

	return (config_munmap == false);
}
