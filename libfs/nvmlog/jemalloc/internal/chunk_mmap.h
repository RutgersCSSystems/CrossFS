/******************************************************************************/
#ifdef JEMALLOC_H_TYPES

#endif /* JEMALLOC_H_TYPES */
/******************************************************************************/
#ifdef JEMALLOC_H_STRUCTS

#endif /* JEMALLOC_H_STRUCTS */
/******************************************************************************/
#ifdef JEMALLOC_H_EXTERNS

void	pages_purge(void *addr, size_t length);

void	*chunk_alloc_mmap(size_t size, size_t alignment, bool *zero);
bool	chunk_dealloc_mmap(void *chunk, size_t size);

//NVRAM changes
void	*nv_chunk_alloc_mmap(size_t size, size_t alignment, bool *zero, 
							void *rqst);


#endif /* JEMALLOC_H_EXTERNS */
/******************************************************************************/
#ifdef JEMALLOC_H_INLINES

#endif /* JEMALLOC_H_INLINES */
/******************************************************************************/
