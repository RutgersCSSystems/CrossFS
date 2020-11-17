#ifndef NV_STRUCTS_H_
#define NV_STRUCTS_H_

#include <inttypes.h>
#include "gtthread_spinlocks.h"


#define VALID 0
#define INVALID 9

typedef unsigned long ULONG;
typedef unsigned int  UINT;

/* Maximum length of each object name */
#define MAXOBJNAMELEN 255

/*Every malloc call will lead to a mmapobj creation*/
struct mmapobj {
	ULONG strt_addr;
	ULONG data_addr;
	unsigned int vma_id;
	ULONG length;
	ULONG offset;
	struct proc_obj *proc_obj;
	rbtree chunkobj_tree;
	int chunk_tree_init;
	int proc_id;
	int numchunks;
	int mmap_offset;
	UINT meta_offset;
	char mmapobjname[MAXOBJNAMELEN];
	/*original persistent proc obj pointer
	used when we only expose read-only versions*/
	ULONG persist_mmapobj;
};
typedef struct mmapobj mmapobj_s;



struct chunkobj{

	UINT length;
	UINT offset;
	UINT chunkid;
	UINT pid;
	UINT vma_id;
	UINT commitsz;
	void *nv_ptr;
	uint8_t valid;
	char objname[MAXOBJNAMELEN];

#ifdef _ENABLE_SWIZZLING
	void *old_nv_ptr;
#endif

#ifdef _USE_TRANSACTION
	void *log_ptr;
	size_t logptr_sz;
	uint8_t dirty;
	struct gt_spinlock_t chunk_lock;
#endif
	//spin lock for transactions
	//struct gt_spinlock_t chunk_lock;

	//Debugging purpose
	//UINT chunk_cnt;
	//int chunk_commit;
	//char varname[64];
	//This pointer contains
	//the nv_ptrs value during
	//previous application run;
	//This is loaded during application
	//restart
	//int logcpy;

#ifdef _VALIDATE_CHKSM
	long checksum;
#endif

	//struct mmapobj *mmapobj;
	//
#ifdef _NVSTATS
	int num_memcpy;
#endif

	//#ifdef _RMT_PRECOPY
	//NVM and DRAM pointer
	//for remote process
	/*void *rmt_nv_ptr;
	void *rmt_log_ptr;
    int rmt_nvdirtchunk;
	void **rmt_armci_ptr;
	int my_rank;
	int my_rmt_chkpt_peer;
	int version;
	void *dummy_ptr;
	int num_rmt_cpy;*/
	//#endif;
};
typedef struct chunkobj chunkobj_s;


/* Each user process will have a process obj
 what about threads??? */
struct proc_obj {
	int pid;
	rbtree mmapobj_tree;
	unsigned int mmapobj_initialized;
	/*starting virtual address of process*/
	ULONG start_addr;
	/*size*/
	ULONG size;
	/*current offset. indicates where is the offset now pointing to */
	//ULONG offset;
	ULONG data_map_size;
	int pesist_mmaps;
	UINT num_objects;
	/*includes persist and non persist type*/
	int num_mmaps;
	UINT meta_offset;
	//needs to be cleared;
	int haslog;
	int haslogMapped;
	/*original persistent proc obj pointer
	used when we only expose read-only versions*/
	ULONG persist_proc_obj;
};
typedef struct proc_obj proc_s;

struct rqst_struct {
	size_t bytes;


	char var_name[MAXOBJNAMELEN];
	//unique id on how application wants to identify
	//this mmapobj;
	int id;
	int pid;
	int ops;
	//ULONG mem;
	unsigned int order_id;
	//buffer if dram used as
	//cache
	void *nv_ptr;
	void *log_ptr;
	size_t logptr_sz;
	//for shadow copy
	int no_dram_flg;


	//volatile flag
	int isVolatile;
	ULONG mmapobj_straddr;
	ULONG offset;
	int access;
	UINT commitsz;

};

typedef struct rqst_struct rqst_s;

struct nvmap_arg_struct {

	ULONG fd;
	ULONG offset;
	int vma_id;
	int proc_id;
	/*flags related to persistent memory usage*/
	int pflags;
	int noPersist; // indicates if this mmapobj is persistent or not
	int ref_count;
};
typedef struct nvmap_arg_struct nvarg_s;

struct mmapobj_nodes {
	ULONG start_addr;
	ULONG end_addr;
	int map_id;
	int proc_id;
	mmapobj_s *mmapobj;
};


struct queue {

	ULONG offset;
	unsigned int num_mmapobjs;
	/*This lock decides the parallel
     nv ram access */
	//struct gt_spinlock_t lock;
	/*Lock used by active memory processing*/
	int outofcore_lock;
	//struct list_head lmmapobj_list;
	int list_initialized;
};

enum PCKT_TYPE_E { PROCESS =1,
	VMA };

/*Remote checkpoint data structure */
struct chkpt_header_struct {
	int pid;
	int type;
	int storeid;
	size_t bytes;
};

typedef struct chkpt_header_struct chkpthead_s;


struct arg_struct {
	int rank;
	int no_procs;
};

struct cktpt_lock{
	struct gt_spinlock_t lock;
	volatile int dirty;
	int siglist;
	int rank;
	int local_chkpt_cnt;
	/*sync lc or async lc*/
	int chkpt_type;

};
typedef struct cktpt_lock cktptlock_s;


#ifdef _NVSTATS
#define MAX_CHUNKS 1024

struct proc_stats_struct {

	int pid;
	int num_mmaps;
	int num_chunks;
	size_t tot_chunksz;
	size_t tot_mmapsz;

#ifdef _USE_CHECKPOINT
	//total chunksize not equal to commited size
	size_t tot_cmtdata;
	long commit_freq;
	long per_step_chkpt_time;
	int chunk_dist[MAX_CHUNKS];
	size_t tot_rd_chunksz;
	int num_rd_chunks;
#endif

};

typedef struct proc_stats_struct s_procstats;

struct rmt_chkpt_stat{

	int pid;
	int num_chkpts;
	int chkpt_chunks;
	size_t chkpt_sz;
	long commit_freq;
	long chkpt_time;
};
typedef struct rmt_chkpt_stat s_rmtchkptstat;

#endif
#endif
