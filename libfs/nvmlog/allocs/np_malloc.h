#include <stdio.h>
#include <stdlib.h>
#include "nv_map.h"

#ifdef __cplusplus
extern "C" {
#endif

void *np_malloc(size_t bytes, struct rqst_struct *rqst);
void np_free(void *ptr);
void* np_realloc(void* oldmem, size_t bytes);
void *np_calloc(size_t, size_t);
void *pnv_malloc(size_t , struct rqst_struct *);
void *pnv_read(size_t bytes, struct rqst_struct *rqst);
#ifdef __cplusplus
};
#endif



