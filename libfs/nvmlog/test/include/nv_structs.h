#include "gtthread_spinlocks.h"

typedef unsigned long ULONG;
typedef unsigned int  UINT;


/*Every malloc call will lead to a mmapobj creation*/
struct mmapobj {

	ULONG strt_addr;
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
};
typedef struct mmapobj mmapobj_s;


struct chunkobj{

	ULONG length;
	ULONG offset;
	UINT chunkid;
	UINT vma_id;
	//DRAM buffer case
	void *nv_ptr;
	void *dram_ptr;

	long checksum;
	struct mmapobj *mmapobj;
    UINT commitsz;
	int chunk_commit;
	int dirty;
	size_t dram_sz;
	//stats
	int num_memcpy;

	//NVM and DRAM pointer
    //for remote process
	void *rmt_nv_ptr;
	void *rmt_dram_ptr;
    int rmt_nvdirtchunk;
	void **rmt_armci_ptr;
	int my_rank;
	int my_rmt_chkpt_peer;
	int version;
	void *dummy_ptr;
	int num_rmt_cpy;

};
typedef struct chunkobj chunkobj_s;


/* Each user process will have a process obj
 what about threads??? */
struct proc_obj {
    int pid;
    struct list_head next_proc;
    struct list_head mmapobj_list;
    //rb_red_blk_tree* mmapobj_tree;
	rbtree mmapobj_tree;
    unsigned int mmapobj_initialized;
    /*starting virtual address of process*/
    ULONG start_addr;
    /*size*/
    ULONG size;
   /*current offset. indicates where is the offset now
    pointing to */
   ULONG offset;
   ULONG data_map_size;
   int num_mmapobjs;
   unsigned int meta_offset;
   int file_desc;

};

typedef struct proc_obj proc_s;

struct rqst_struct {
    size_t bytes;
    char *var_name;
    //unique id on how application wants to identify
    //this mmapobj;
    int id;
    int pid;
    int ops;
    ULONG mem;
    unsigned int order_id;
	//buffer if dram used as
	//cache
    void *nv_ptr;
    void *dram_ptr;
    size_t dram_sz;
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
};


struct queue {

    ULONG offset;
    unsigned int num_mmapobjs;
    /*This lock decides the parallel
     nv ram access */
    //struct gt_spinlock_t lock;
    /*Lock used by active memory processing*/
    int outofcore_lock;
    struct list_head lmmapobj_list;
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
	//total chunksize not equal to commited size
	size_t tot_cmtdata;
	long commit_freq;
        long per_step_chkpt_time;
	int chunk_dist[MAX_CHUNKS];

	size_t tot_rd_chunksz;
	int num_rd_chunks;

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

