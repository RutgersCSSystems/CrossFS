/*
 * nv_map.h
 *
 *  Created on: Mar 20, 2011
 *      Author: sudarsun
 */

#ifndef NV_MAP_H_
#define NV_MAP_H_


#include "list.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include "rbtree.h"
#include "nv_structs.h"
#include "error_codes.h"
#ifdef __cplusplus
extern "C" {
#endif

enum CHUNKFLGS { PROCESSED =1};




/*struct queue *l_queue;
//Function to read nv queue
struct queue *get_queue(void);
//Function to get next mmapobj from queue
struct mmapobj* get_nxt_mmapobj(struct queue*,struct mmapobj*);
//Function to get the memory location of the mmapobj
void *get_mmapobj_mem( struct mmapobj *mmapobj, int i);
//gets the memory location from queue that needs to be processed
void *get_next_queue_item( struct rqst_struct *rqst, int index,int rank );
//Gets the total queue mmapobjs currently present in queue
unsigned int get_num_queue_mmapobjs(int timestep);
//This function increments outof core processing
//to enable outof core processing
int incr_outof_core_lock(void);
//This function decrements outof core processing
//to disable outof core processing
int decr_outof_core_lock(void);
//This function enable out of core processing
//by enabling the mmapobj queue lock
int is_outof_core_lock_disabled(void);
//function to print the out of core lock
//value
int print_outof_core_lock(void);*/


//Function to check if mmapobj queue is already initialized
//FIXME: if multiple queues existis, then it should have a
// identifier
void* check_if_init();
//void* nv_mmapobj(struct rqst_struct *);
void* create_new_process(struct rqst_struct *rqst);
void* nv_map_read(struct rqst_struct *, void *);
int nv_commit(struct rqst_struct *);
int nv_munmap(void *addr);
unsigned int generate_vmaid(const char *key);
ULONG findoffset(UINT proc_id, ULONG curr_addr);
int nv_record_chunk(rqst_s *rqst, ULONG);
void nvmalloc(void);

//Should be first called
int initialize_nv(int sema);
/*Queue related changes */
//first time initialization
//semaphore intialization
void *create_queue();
/*intialize library*/
int intialize(int pid);
/*check if queue exists */
void* check_if_init();
/*Debugging functions */
void print_mmapobj(struct mmapobj *mmapobj);
/*get process start address */
ULONG  get_proc_strtaddress(struct rqst_struct *);
/*function to get process mmmapobj num*/
int get_proc_num_maps(int pid);
/*function to initialize process nv structure*/
int nv_initialize(struct rqst_struct *rqst);

void *map_nvram_state(rqst_s *rqst);

proc_s* read_map_from_pmem(int pid);

int  setup_map_file(char *filepath, ULONG bytes);

int nv_chkpt_all(rqst_s *, int);

proc_s* load_process(int pid, int perm);

static inline void PRINT(const char* format, ... ) {

    va_list args;
    va_start( args, format );
    fprintf( stderr, format, args );
    va_end( args );
}

void* _mmap(void *addr, size_t size, int mode, int prot, int fd, 
			int offset, nvarg_s *);

int locate_mmapobj_node(void *addr, struct rqst_struct *, ULONG *);

void* proc_rmt_chkpt(int procid, size_t *bytes, int chkdirt, int, int);

int reg_for_signal(int procid);

int init_checkpoint(int procid);

size_t nv_disablprot(void *addr, int *chunkid);

int set_chunkprot();

int start_asyn_lcl_chkpt(int chunkid);

int add_to_fault_lst(int id);
int start_asyn_lcl_chkpt(int chunkid);

int enable_chunkprot(int);

int clear_fault_lst();


long get_chkpt_itr_time();


#ifdef _NVSTATS
int add_stats_chunk(int pid, size_t chunksize);
int add_stats_mmap(int pid, size_t mmap_size);
int add_stats_commit_freq(int pid, long time);
int add_stats_chkpt_time(int pid, long time);
int add_to_chunk_memcpy(chunkobj_s *);
int print_stats(int pid); 
int add_stats_chunk_read(int pid, size_t chunksize);
#endif

int rmtchkpt_setup_memory(int, int);
int nv_register_chkpt_peer(int);

int app_exec_finish(int);


#ifdef __cplusplus
};  /* end of extern "C" */
#endif


#endif


