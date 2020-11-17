/*
 * nv_debug.c
 *
 *  Created on: Mar 26, 2013
 *      Author: hendrix
 */

#include "nv_debug.h"

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <pthread.h>

//#define _NVDEBUG
void DEBUG(const char* format, ... ) {
#ifdef _NVDEBUG
		FILE *fp;
        va_list args;
        va_start( args, format );
        vfprintf(stderr, format, args );
        //printf(format, args );
        va_end( args );
#endif
}

//Trouble shooting debug
//use only if trouble shooting
//some specific functions
void DEBUG_T(const char* format, ... ) {

#ifdef _NVDEBUG
		FILE *fp;
        va_list args;
        va_start( args, format );
        vfprintf( stdout, format, args );
        va_end( args );
#endif
}


void DEBUG_PROCOBJ(proc_s *proc_obj) {
#ifdef _NVDEBUG

	fprintf(stdout, "proc_obj->pid %d \n", proc_obj->pid);
	fprintf(stdout, "proc_obj->size %lu \n", proc_obj->size);
	fprintf(stdout, "proc_obj->start_addr %lu\n", proc_obj->start_addr);
#endif
}

void DEBUG_MMAPOBJ(mmapobj_s *mmapobj) {
#ifdef _NVDEBUG
	fprintf(stdout,"----------------------\n");
	fprintf(stdout,"mmapobj: vma_id %u\n", mmapobj->vma_id);
	fprintf(stdout,"mmapobj: length %ld\n",  mmapobj->length);
	fprintf(stdout,"mmapobj: proc_id %d\n", mmapobj->proc_id);
	fprintf(stdout,"mmapobj: offset %ld\n", mmapobj->offset);
	fprintf(stdout,"mmapobj: mmapoffset %ld\n", mmapobj->mmap_offset);
	fprintf(stdout,"mmapobj: numchunks %d \n",mmapobj->numchunks);
	fprintf(stdout,"----------------------\n");
#endif
}

void DEBUG_MMAPOBJ_T(mmapobj_s *mmapobj) {

#ifdef _NVDEBUG
	fprintf(stdout,"----------------------\n");
	fprintf(stdout,"mmapobj: vma_id %u\n", mmapobj->vma_id);
	fprintf(stdout,"mmapobj: length %ld\n",  mmapobj->length);
	fprintf(stdout,"mmapobj: proc_id %d\n", mmapobj->proc_id);
	fprintf(stdout,"mmapobj: offset %ld\n", mmapobj->offset);
	fprintf(stdout,"mmapobj: mmapoffset %ld\n", mmapobj->mmap_offset);
	fprintf(stdout,"mmapobj: numchunks %d \n",mmapobj->numchunks);
	fprintf(stdout,"----------------------\n");
#endif
}

void DEBUG_CHUNKOBJ(chunkobj_s *chunkobj) {
#ifdef _NVDEBUG
	fprintf(stdout,"----------------------\n");
	fprintf(stdout,"chunkobj: chunkid %u\n", chunkobj->chunkid);
	fprintf(stdout,"chunkobj: length %ld\n", chunkobj->length);
	fprintf(stdout,"chunkobj: vma_id %d\n", chunkobj->vma_id);
	fprintf(stdout,"chunkobj: offset %ld\n", chunkobj->offset);
	fprintf(stdout,"chunkobj: nvptr %lu\n", (ULONG)chunkobj->nv_ptr);
#ifdef VALIDATE_CHKSM
	fprintf(stdout,"chunkobj: checksum %ld\n",  chunkobj->checksum);
#endif
#ifdef _USE_SHADOWCOPY
	fprintf(stdout,"chunkobj: log_ptr %lu\n", chunkobj->log_ptr);
#endif
	fprintf(stdout,"chunkobj: varname %s\n", chunkobj->objname);
	fprintf(stdout,"----------------------\n");
#endif
}

void DEBUG_CHUNKOBJ_T(chunkobj_s *chunkobj) {

#ifndef _NVDEBUG
	fprintf(stdout,"----------------------\n");
	fprintf(stdout,"chunkobj: chunkid %u\n", chunkobj->chunkid);
	fprintf(stdout,"chunkobj: length %u\n", chunkobj->length);
	fprintf(stdout,"chunkobj: vma_id %u\n", chunkobj->vma_id);
	fprintf(stdout,"chunkobj: offset %u\n", chunkobj->offset);
	fprintf(stdout,"chunkobj: nvptr %lu\n", (ULONG)chunkobj->nv_ptr);
	fprintf(stdout,"----------------------\n");
#endif
}

