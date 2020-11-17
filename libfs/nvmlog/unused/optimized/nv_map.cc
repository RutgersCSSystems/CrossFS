#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <strings.h>
#include <time.h>
#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <functional>
#include <queue>
#include <list>

#include "nv_map.h"
#include "nv_def.h"
#include "checkpoint.h"
#include "util_func.h"
#include "time_delay.h"
#include "jemalloc/jemalloc.h"
#include "LogMngr.h"
#include "nv_transact.h"

//#include "google_hash.h"

#ifdef _USEPIN
#include "pin_mapper.h"
#endif

using namespace std;

//using namespace snappy;

///////////////////////glob vars///////////////////////////////////////////////////
//checkpoint related code
pthread_mutex_t chkpt_mutex = PTHREAD_MUTEX_INITIALIZER;
int mutex_set = 0;
pthread_cond_t dataPresentCondition = PTHREAD_COND_INITIALIZER;
int  dataPresent=0;
/*remote chkpt code uses
 * this to check frequency
 * of checkpoint*/
int local_chkpt_cnt=0;
int dummy_var = 0;
/*List containing all project id's */
//static struct list_head proc_objlist;
/*intial process obj list var*/
int proc_list_init = 0;
/*fd for file which contains process obj map*/
static int proc_map;
ULONG proc_map_start;
int g_file_desc = -1;

nvarg_s nvarg;
rbtree map_tree;
rbtree proc_tree;


long chkpt_itr_time;

std::map <int,int> fault_chunk, fault_hist;
std::map <int,int>::iterator fault_itr;
int stop_history_coll, checpt_cnt;

#ifdef _NVSTATS
s_procstats proc_stat;
UINT total_mmaps;
struct timeval commit_start, commit_end;
#endif

#ifdef _USE_FAULT_PATTERNS
int chunk_fault_lst_freeze;
#endif

/*contains a list of all pointers allocated
//during a session. This map is loaded during
//restart, to know what where the previous
//pointers and where did they map to*/
std::map <void *, chunkobj_s*> pntr_lst;

/*enabled if _USE_CACHEFLUSH
 * is enabled
 *  */
uint8_t useCacheFlush;

#ifdef _NVRAM_OPTIMIZE
proc_s* prev_proc_obj = NULL;
UINT prev_proc_id;

#define NUM_MMAP_CACHE_CNT 128
ULONG mmap_strt_addr_cache[NUM_MMAP_CACHE_CNT];
UINT mmap_size_cache[NUM_MMAP_CACHE_CNT];
ULONG mmap_ref_cache[NUM_MMAP_CACHE_CNT];
UINT next_mmap_entry;
uint8_t use_map_cache;
#endif


///////////////////////MACROS///////////////////////////////////////////////////

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)


static void handler(int sig, siginfo_t *si, void *unused)
{

  fprintf(stdout,"recvd seg fault %lu\n", (unsigned long)si->si_addr);
  load_valid_addr(&si->si_addr);
}


static void initseghandling()
{

   struct sigaction sa;
   struct sched_param param;
   sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    //if (sigaction(SIGSEGV, &sa, NULL) == -1)
       //  handle_error("sigaction");
}


int file_error(char *filename) {

  fprintf(stdout,"error opening file %s \n", filename);
  return 0;
}

int CompRange(node key_node, void* a, void* b) {

  /*rb_red_blk_node *node = (rb_red_blk_node *)c;*/
  struct mmapobj_nodes *mmapobj_struct=
      (struct mmapobj_nodes *)key_node->value;

  ULONG a_start_addr = (ULONG)a; 
  ULONG b_start_addr = (ULONG)b; 

  if((a_start_addr > b_start_addr) &&
      ( mmapobj_struct->end_addr > a_start_addr))
  {
#ifdef _NVDEBUG_L2
    fprintf(stdout, "a_start_addr %lu, b_start_addr %lu," 
        "mmapobj_struct->end_addr %lu mmpaid %d\n",
        a_start_addr, b_start_addr, mmapobj_struct->end_addr,
        mmapobj_struct->map_id);
#endif
    return(0);
  }else{
#ifdef _NVDEBUG_L2
    fprintf(stdout,"Compare range: mapid %u"
        " a_start_addr %lu "
        "b_start_addr %lu "
        "mmapobjstruct.end_addr %lu \n",
        mmapobj_struct->map_id,
        a_start_addr,
        b_start_addr,
        mmapobj_struct->end_addr);
#endif
  }
  if(a_start_addr > b_start_addr) return(1);
  if( a_start_addr < b_start_addr) return(-1);
  return(0);
}

int IntComp(node n, void* a, void* b) {
  if( (uintptr_t)a > (uintptr_t)b) return(1);
  if( (uintptr_t)a < (uintptr_t)b) return(-1);
  return(0);
}


#ifdef _USEPIN
void init_pin() {
  CreateSharedMem();
}
#endif


#ifdef _USEPIN
int num_maps;
#endif

void *mmap_wrap(void *addr, size_t size, 
    int mode, int prot, int fd,
    size_t offset, nvarg_s *s) {

  void* nvmap;

#ifdef _NVDEBUG
  printf("nvarg.proc_id %d %d %d\n",s->proc_id, s->vma_id, s->pflags);
#endif

#ifndef _USE_FAKE_NVMAP
  //nvmap = (void *)syscall(__NR_nv_mmap_pgoff,addr,size,mode,prot, s);
   nvmap = (void *)mmap(addr, size, mode, prot, fd, 0);
#else
  nvmap = (void *)mmap(addr, size, mode, prot, fd, 0);
#endif

#ifdef _USEPIN
  //creates shared memory. if shared memory already created
  // then returns pointer
  init_pin();
  num_maps++;
  //printf("Writing line %lu %lu %d\n", nvmap,nvmap+size,num_maps);
  Writeline((unsigned long)nvmap, (unsigned long)nvmap+size);
#endif
  return nvmap; 
}

//RBtree code ends
int initialize_mmapobj_tree( proc_s *proc_obj){

  assert(proc_obj);
  if(!proc_obj->mmapobj_tree) {
    proc_obj->mmapobj_tree =rbtree_create();
    assert(proc_obj->mmapobj_tree);
    proc_obj->mmapobj_initialized = 1;
  }
  return 0;
}

int init_chunk_tree( mmapobj_s *mmapobj){

  assert(mmapobj);
  mmapobj->chunkobj_tree =rbtree_create();
  mmapobj->chunk_tree_init = 1;
  return 0;
}

void print_mmapobj(mmapobj_s *mmapobj) {

  fprintf(stdout,"----------------------\n");
  fprintf(stdout,"mmapobj: vma_id %u\n", mmapobj->vma_id);
  fprintf(stdout,"mmapobj: length %ld\n",  mmapobj->length);
  fprintf(stdout,"mmapobj: proc_id %d\n", mmapobj->proc_id);
  fprintf(stdout,"mmapobj: offset %ld\n", mmapobj->offset);
  fprintf(stdout,"mmapobj: numchunks %d \n",mmapobj->numchunks); 
  fprintf(stdout,"----------------------\n");
}

void print_chunkobj(chunkobj_s *chunkobj) {

  fprintf(stdout,"----------------------\n");
  fprintf(stdout,"chunkobj: chunkid %u\n", chunkobj->chunkid);
  fprintf(stdout,"chunkobj: length %ld\n", chunkobj->length);
  fprintf(stdout,"chunkobj: vma_id %d\n", chunkobj->vma_id);
  fprintf(stdout,"chunkobj: offset %ld\n", chunkobj->offset);
  fprintf(stdout,"chunkobj: nvptr %lu\n", (ULONG)chunkobj->nv_ptr); 
#ifdef _VALIDATE_CHKSM
  fprintf(stdout,"chunkobj: checksum %ld\n",  chunkobj->checksum);
#endif
#ifdef _USE_SHADOWCOPY
  fprintf(stdout,"chunkobj: log_ptr %lu\n", chunkobj->log_ptr);
#endif
  fprintf(stdout,"chunkobj: varname %s\n", chunkobj->varname);
  fprintf(stdout,"----------------------\n");
}

int copy_chunkoj(chunkobj_s *dest, chunkobj_s *src) {

  assert(dest);

  dest->pid = src->pid;
  dest->chunkid = src->chunkid;
  dest->length =  src->length;
  dest->vma_id = src->vma_id;
  dest->offset = src->offset;

  return 0;
}


int copy_mmapobj(mmapobj_s *dest, mmapobj_s *src) {

  assert(dest);

  dest->vma_id = src->vma_id;
  dest->length = src->length;
  dest->proc_id = src->proc_id;
  dest->offset  = src->offset;
  dest->numchunks = src->numchunks;

  return 0;
}

/*creates mmapobj object.sets object variables to app. value*/
static mmapobj_s* create_mmapobj(rqst_s *rqst, 
    ULONG curr_offset, proc_s* proc_obj,
    ULONG data_addr) {

  mmapobj_s *mmapobj = NULL;
  ULONG addr = 0;

  assert(rqst);
  assert(proc_obj);
  assert(proc_map_start);

  addr = proc_map_start;
  addr = addr + proc_obj->meta_offset;
  mmapobj = (mmapobj_s*) addr;
  proc_obj->meta_offset += sizeof(mmapobj_s);

  mmapobj->vma_id =  rqst->id;
  mmapobj->length = rqst->bytes;
  mmapobj->proc_id = rqst->pid;
  mmapobj->offset = curr_offset;
  mmapobj->data_addr = data_addr;
  rqst->id = BASE_METADATA_NVID + mmapobj->vma_id;
  rqst->bytes = MMAP_METADATA_SZ;

#ifdef _USE_FAKE_NVMAP
  char file_name[256];
  char fileid_str[64];
  bzero(file_name,256);
  generate_file_name((char *) PROCMAPMETADATA_PATH,rqst->pid, file_name);
  sprintf(fileid_str, "%d", rqst->id);
  strcat(file_name,"_");
  strcat(file_name, fileid_str);
  int fd = setup_map_file(file_name, rqst->bytes);
  if (fd == -1) {
    //file_error( file_name);
    perror("file open error\n");
    return NULL;
  }
#ifdef _NVDEBUG
  //addr = (ULONG) mmap(0, rqst->bytes,
  //        PROT_NV_RW, MAP_SHARED, fd, 0);
  fprintf(stdout,"creating mmap obj %s of size %u\n",
      file_name,rqst->bytes);
#endif
  addr = (ULONG) mmap(0, rqst->bytes,
      PROT_NV_RW, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
  assert((void *)addr != MAP_FAILED);
#else
  addr = (ULONG)(map_nvram_state(rqst));
#endif
  assert(addr);
  //OPTIMIZE_CHANGE  
  //memset((void *)addr,0, rqst->bytes);
  mmapobj->strt_addr = addr;

#ifdef _NVDEBUG
  fprintf(stdout, "Setting offset mmapobj->vma_id"
      " %u to %u  %u\n",mmapobj->vma_id, 
      mmapobj->offset, proc_obj->meta_offset);
#endif
  init_chunk_tree(mmapobj);
  assert(mmapobj->chunkobj_tree);

  return mmapobj;
}


static void update_chunkobj(
      rqst_s *rqst, 
      mmapobj_s* mmapobj,
      chunkobj_s *chunkobj) {

  assert(chunkobj);
  chunkobj->pid =  rqst->pid;
  chunkobj->chunkid =  rqst->id;
  chunkobj->length = rqst->bytes;
  chunkobj->vma_id = mmapobj->vma_id;
  chunkobj->offset = rqst->offset;
  chunkobj->mmapobj = mmapobj;
  chunkobj->dirty = 0;
  chunkobj->nv_ptr = rqst->nv_ptr;
  
  #ifdef _USE_SHADOWCOPY
  chunkobj->logcpy = 0;
  chunkobj->log_ptr = rqst->log_ptr;
  chunkobj->logptr_sz = rqst->logptr_sz;
  assert(chunkobj->log_ptr);
  //assert(chunkobj->logptr_sz);
  #endif
}

/*creates mmapobj object.sets object variables to app. value*/
static chunkobj_s* create_chunkobj(rqst_s *rqst, mmapobj_s* mmapobj) {

  chunkobj_s *chunkobj = NULL;
  ULONG addr = 0;
  UINT mapoffset =0;
  void *ptr;

  assert(rqst);
  addr = mmapobj->strt_addr;
  assert(addr);
  mapoffset = mmapobj->meta_offset;
  addr = addr + mapoffset;
  chunkobj = (chunkobj_s*)addr;
  mapoffset += sizeof(chunkobj_s);
  update_chunkobj(rqst, mmapobj, chunkobj);
  mmapobj->meta_offset = mapoffset;
  ptr = (void *)(addr + mapoffset);
  ptr = NULL;

#ifdef _USE_TRANSACTION
  //initialize the chunk dirty bit
  chunkobj->dirty = 0;
#endif

#ifdef  _NVSTATS
  add_stats_chunk(rqst->pid, rqst->bytes);
#endif

#ifdef _NVDEBUG
  fprintf(stdout, "Setting chunkid %d vma_id"
      " %u at offset %u and mmap offset %u \n",chunkobj->chunkid, 
      chunkobj->vma_id, chunkobj->offset, mmapobj->meta_offset);
#endif

  return chunkobj;
}


/*Function to return the process object to which mmapobj belongs
@ mmapobj: process to which the mmapobj belongs
@ return: process object 
 */
proc_s* get_process_obj(mmapobj_s *mmapobj) {

  if(!mmapobj) {
    fprintf(stdout, "get_process_obj: mmapobj null \n");
    return NULL;
  }
  return mmapobj->proc_obj;
}


chunkobj_s* find_chunk(mmapobj_s *t_mmapobj, unsigned int chunkid) {

  chunkobj_s *chunkobj = NULL;
  if(t_mmapobj) {
    if(t_mmapobj->chunkobj_tree) {
      chunkobj  = (chunkobj_s *)rbtree_lookup(t_mmapobj->chunkobj_tree,
          (void*)chunkid, IntComp);
      if(chunkobj){
        return chunkobj;
      }
    }
  }
  return NULL;
}

int  find_vmaid_from_chunk(rbtree_node n, unsigned int chunkid) {

  int ret =0;
  mmapobj_s *t_mmapobj = NULL;
  chunkobj_s *chunkobj = NULL;

  //assert(n);
  if (n == NULL) {
    return 0;
  }

  if (n->right != NULL) {
    ret = find_vmaid_from_chunk(n->right, chunkid);
    if(ret)
      return ret;
  }
  t_mmapobj = (mmapobj_s *)n->value;
  chunkobj  = find_chunk(t_mmapobj, chunkid);
  if(chunkobj){
    return t_mmapobj->vma_id;
  }

  if (n->left != NULL) {
    return find_vmaid_from_chunk(n->left, chunkid);
  }

  return 0;
}

/*Function to find the mmapobj.
@ process_id: process identifier
@ var:  variable which we are looking for
 */
mmapobj_s* find_mmapobj_from_chunkid(unsigned int chunkid, proc_s *proc_obj ) {

  mmapobj_s *t_mmapobj = NULL;
  ULONG vmid_l = 0;  

  if(!proc_obj) {
    fprintf(stdout, "could not identify project id \n");
    return NULL;
  }
  /*if mmapobj is not yet initialized do so*/
  if (!proc_obj->mmapobj_initialized){
    fprintf(stdout,"proc_obj->mmapobj_initialized failed \n");
    initialize_mmapobj_tree(proc_obj);
    return NULL;
  }
  if(proc_obj->mmapobj_tree) {

    //print_tree(proc_obj->mmapobj_tree);
    vmid_l = find_vmaid_from_chunk(proc_obj->mmapobj_tree->root,chunkid);  
    if(!vmid_l){ 
      goto exit;
    }
    t_mmapobj =(mmapobj_s*)rbtree_lookup(proc_obj->mmapobj_tree,
        (void *)vmid_l, IntComp);
  }

exit:
#ifdef _NVDEBUG
  if(t_mmapobj)
    fprintf(stdout, "find_mmapobj chunkid %u in vmaid %u \n", chunkid, vmid_l);
  else {
    fprintf(stdout,"mmapobj find failed for chunk  %u in vmaid %u \n", chunkid, vmid_l);
  }
#endif
  return t_mmapobj;
}


mmapobj_s* find_mmapobj(UINT vmid_l, proc_s *proc_obj ) {

  mmapobj_s *t_mmapobj = NULL;
  if(!proc_obj) {
    fprintf(stdout, "could not identify project id \n");
    return NULL;
  }
  /*if mmapobj is not yet initialized do so*/
  if (!proc_obj->mmapobj_initialized){
    initialize_mmapobj_tree(proc_obj);
    return NULL;
  }
  t_mmapobj =(mmapobj_s*)rbtree_lookup(proc_obj->mmapobj_tree,
      (void *)vmid_l, IntComp);
#ifdef _NVDEBUG
  if(t_mmapobj)
    fprintf(stdout, "find_mmapobj found t_mmapobj %u \n", t_mmapobj->vma_id);
#endif
  return t_mmapobj;
}


/*add the mmapobj to process object*/
static int add_mmapobj(mmapobj_s *mmapobj, proc_s *proc_obj) {

  if (!mmapobj)
    return 1;

  if (!proc_obj)
    return -1;

  if (!proc_obj->mmapobj_initialized)
    initialize_mmapobj_tree(proc_obj);

  //RB tree code
  assert(proc_obj->mmapobj_tree);
  rbtree_insert(proc_obj->mmapobj_tree, (void*)mmapobj->vma_id,
      mmapobj, IntComp);
  //set the process obj to which mmapobj belongs
  mmapobj->proc_obj = proc_obj;

   if(use_map_cache)
     add_to_mmap_cache( mmapobj);
  /*
   * When ever a mmap obj is added flush to
   * NVM
   */
  if(useCacheFlush){
    flush_cache(mmapobj, sizeof(mmapobj_s));
  }


  return 0;
}

/*add the mmapobj to process object*/
static int add_chunkobj(mmapobj_s *mmapobj, chunkobj_s *chunk_obj) {

  if (!mmapobj)
    return -1;

  if (!chunk_obj)
    return -1;

  #ifdef _NVDEBUG
  fprintf(stdout,"add_chunkobj: chunkid %d"
      "chunk_tree_init %d \n",(void*)chunk_obj->chunkid,
      mmapobj->chunk_tree_init);
  #endif

  //OPTIMIZE CHANGE
  /*if (!mmapobj->chunk_tree_init){
    init_chunk_tree(mmapobj);
  }
  assert(mmapobj->chunkobj_tree);*/

  rbtree_insert(mmapobj->chunkobj_tree, (void*)(chunk_obj->chunkid),
      (void*)chunk_obj, IntComp);

  //set the process obj to which mmapobj belongs
  //chunk_obj->mmapobj = mmapobj;

  /*we flush the newly created
   * chunk object*/
  if(useCacheFlush){
    if(chunk_obj)
      flush_cache(chunk_obj, sizeof(chunkobj_s));
  }


  return 0;
}

int restore_chunk_objs(mmapobj_s *mmapobj, int perm){

  rqst_s rqst, datarq;
  chunkobj_s *nv_chunkobj = NULL;
  chunkobj_s *chunkobj = NULL;

  void *mem = NULL;
  int idx = 0;
  void *addr = NULL;
  unsigned long addr_l = 0;

  assert(mmapobj);
  rqst.id = BASE_METADATA_NVID + mmapobj->vma_id;
  rqst.pid = mmapobj->proc_id;
  rqst.bytes = mmapobj->length;

#ifdef _USE_FAKE_NVMAP
  char file_name[256];
  char fileid_str[64];
  bzero(file_name,256);
  generate_file_name((char *) PROCMAPMETADATA_PATH,rqst.pid, file_name);
  sprintf(fileid_str, "%d", rqst.id);
  strcat(file_name,"_");
  strcat(file_name, fileid_str);

  int fd = open(file_name,O_RDWR);// setup_map_file(file_name,rqst.bytes);
  if (fd == -1) {
    file_error( file_name);
    return -1;
  }
  mem = (void *) mmap(0, rqst.bytes,
      PROT_NV_RW, MAP_SHARED, fd, 0);
#else
  mem = map_nvram_state(&rqst);
#endif
  assert(mem);

  //update start addr
  //How are these different?
  mmapobj->strt_addr = (ULONG)mem;
  mmapobj->chunk_tree_init = 0;
  addr = (void *)mem;

  //Now MAP for data
  datarq.id = mmapobj->vma_id;
  datarq.pid =mmapobj->proc_id;
  datarq.bytes= mmapobj->length;
  mmapobj->data_addr = (ULONG)map_nvram_state(&datarq);
  assert(mmapobj->data_addr);

  for (idx = 0; idx < mmapobj->numchunks; idx++) {

    nv_chunkobj = (chunkobj_s*)addr;
    if(check_modify_access(perm)) {

      chunkobj = nv_chunkobj;
      /*this wont work for restart. we need to initialize nvptr*/
#ifdef _USE_SHADOWCOPY
      chunkobj->log_ptr =malloc(chunkobj->logptr_sz);
      chunkobj->nv_ptr = 0;
#ifdef _USE_UNDO_LOG
      assert(chunkobj->nv_ptr);
      record_chunks(chunkobj->nv_ptr,chunkobj);
#else
      assert(chunkobj->log_ptr);
      record_chunks(chunkobj->log_ptr,chunkobj);
#endif

#endif
    }else {
      chunkobj = nv_chunkobj;
    }
    //add the chunk objects to pointer tracing list
    //this is useful for tracing pointers during
    //recovery.
    //Beware: if the nv_ptr is null, assert will be false
    create_chunk_record(chunkobj->nv_ptr, chunkobj);

    //update the chunk pointer to new address
    chunkobj->nv_ptr = (void *)mmapobj->data_addr + chunkobj->offset;

    //add the chunk obj to mmap obj
    assert(add_chunkobj(mmapobj, chunkobj) == 0);

    //increment by chunk metadata size
    addr += (ULONG)sizeof(chunkobj_s);
  }
  return 0;
}

/*Idea is to have a seperate process map file for each process
 But fine for now as using browser */
static proc_s * create_proc_obj(int pid) {

  proc_s *proc_obj = NULL;
  size_t bytes = sizeof(proc_s);
  char file_name[256];


  /*bzero(file_name, 256);
  generate_file_name((char *)PROCMETADATA_PATH, pid, file_name);
  proc_map = setup_map_file(file_name, PROC_METADAT_SZ);
  if (proc_map < 1) {
    printf("failed to create a map using file %s\n",
        file_name);
    goto err_prjcreate;
  }
  proc_obj = (proc_s *) mmap(0, PROC_METADAT_SZ, PROT_NV_RW,
      MAP_SHARED, proc_map, 0);*/

  proc_obj = (proc_s *) mmap(0, PROC_METADAT_SZ, PROT_NV_RW,
      MAP_ANONYMOUS|MAP_PRIVATE, proc_map, 0);
  if (proc_obj == MAP_FAILED) {
    close(proc_map);
    fprintf(stdout,"Error mmapping the file %s\n",file_name);

  }
  memset ((void *)proc_obj,0,bytes);
  proc_map_start = (ULONG) proc_obj;


  return proc_obj;

err_prjcreate:
  return NULL;

}



/*Func resposible for locating a process object given
 process id. The idea is that once we have process object
 we can get the remaining mmaps allocated process object
 YET TO COMPLETE */
static proc_s *find_proc_obj(int proc_id) {

  proc_s *proc_obj = NULL;

#ifdef _NVRAM_OPTIMIZE
  if(proc_id && ((UINT)proc_id == prev_proc_id)  ) {  

    if(prev_proc_obj) {
#ifdef _NVDEBUG
      fprintf(stdout, "returning from cache \n");
#endif
      return prev_proc_obj;
    }
  }
#endif

  if (!proc_list_init) {
    proc_tree = rbtree_create();
    proc_list_init = 1;
    return NULL;
  }

  proc_obj = (proc_s *)rbtree_lookup(
      proc_tree,(void *)proc_id,IntComp);
#ifdef _NVRAM_OPTIMIZE
  prev_proc_obj = proc_obj;
#endif

  return proc_obj;
}



/*Every NValloc call creates a mmap and each mmap
 is added to process object list*/
mmapobj_s* add_mmapobj_to_proc(proc_s *proc_obj, 
    rqst_s *rqst, ULONG offset,
    ULONG data_addr) {

  mmapobj_s *mmapobj = NULL;

  mmapobj = create_mmapobj( rqst, offset, proc_obj, data_addr);
  assert(mmapobj);

  /*add mmap to process object*/
  add_mmapobj(mmapobj, proc_obj);
  proc_obj->num_mmapobjs++;

  return mmapobj;
}

static chunkobj_s* add_chunk_to_mmapobj(mmapobj_s *mmapobj, proc_s *proc_obj,
    rqst_s *rqst) {

  
  chunkobj_s *chunkobj = NULL;
  chunkobj = create_chunkobj( rqst, mmapobj);
  assert(chunkobj);

  /*add mmap to process object*/
  add_chunkobj(mmapobj,chunkobj);
  mmapobj->numchunks++;
#ifdef _NVDEBUG
  print_chunkobj(chunkobj);
#endif

  return chunkobj;
}


/*add process to the list of processes*/
static int add_proc_obj(proc_s *proc_obj) {

  if (!proc_obj)
    return 1;

  //if proecess list is not initialized,
  //then intialize it
  if (!proc_list_init) {
    proc_tree = rbtree_create();
    proc_list_init = 1;
  }
  rbtree_insert(proc_tree,(void *)proc_obj->pid,proc_obj, IntComp);


  /*we flush the proc obj*/
  if(useCacheFlush){
    flush_cache(proc_obj, sizeof(proc_s));
  }

  return 0;
}

/*Function to find the process.
@ pid: process identifier
 */
proc_s* find_process(int pid) {

  proc_s* t_proc_obj = NULL;

  if(!proc_list_init){
    return NULL;
  }
  t_proc_obj = (proc_s*)rbtree_lookup(proc_tree,(void *)pid, IntComp);

#ifdef _NVDEBUG
  if(t_proc_obj)
    fprintf(stdout, "find_mmapobj found t_mmapobj %u \n", t_proc_obj->pid);
#endif
  return t_proc_obj;

}

//Return the starting address of process
ULONG  get_proc_strtaddress(rqst_s *rqst){

  proc_s *proc_obj=NULL;
  int pid = -1;
  uintptr_t uptrmap;

  pid = rqst->pid;
  proc_obj = find_proc_obj(pid);
  if (!proc_obj) {
    fprintf(stdout,"could not find the process. Check the pid %d\n", pid);
    return 0;  
  }else {
    //found the start address
    uptrmap = (uintptr_t)proc_obj->start_addr;
    return proc_obj->start_addr;
  }
  return 0;
}



/*Initializes various process flags
 */
int nv_initialize(void) {


#ifdef _USE_CACHEFLUSH
  useCacheFlush=1;
#else
  useCacheFlush=0;
#endif

#ifdef _NVRAM_OPTIMIZE
  use_map_cache=1;
  next_mmap_entry = 0;
#else
  use_map_cache = 0;  
#endif

}


//creates new  process structure
void* create_new_process(rqst_s *rqst) {

  int pid = -1;
  proc_s *proc_obj=NULL;
  ULONG bytes = 0;
  char *var = NULL;
  char file_name[256];

  assert(rqst);
  bytes = rqst->bytes;
  var = (char *)rqst->var_name;
  pid = rqst->pid;

  /*Before we create a new process
   * we initialize various flags
   */
  nv_initialize();

  proc_obj = create_proc_obj(rqst->pid);
  assert(proc_obj);
  proc_obj->pid = rqst->pid;
  proc_obj->size = 0;
  proc_obj->num_mmapobjs = 0;
  proc_obj->start_addr = 0;
  proc_obj->offset = 0;
  proc_obj->meta_offset = sizeof(proc_s);
  proc_obj->data_map_size += bytes;
  add_proc_obj(proc_obj);


#ifdef _NVDEBUG
  fprintf(stdout,"creating new proc %d\n", proc_obj->pid);
#endif
  return (void *)proc_obj;
}


/*Function to copy mmapobjs */
static int mmapobj_copy(mmapobj_s *dest, mmapobj_s *src){ 

  if(!dest ) {
    printf("mmapobj_copy:dest is null \n");
    goto null_err;
  }

  if(!src ) {
    printf("mmapobj_copy:src is null \n");
    goto null_err;
  }

  //TODO: this is not a great way to copy structures
  dest->vma_id = src->vma_id;
  dest->length = src->length;
  dest->proc_id = src->proc_id;
  dest->offset = src->offset;
  //print_mmapobj(dest);

#ifdef CHCKPT_HPC
  //FIXME:operations should be structure
  // BUT how to map them
  dest->order_id = src->order_id;
  dest->ops = src->ops;
#endif
  return 0;
  null_err:
  return -1;  
}


/*gives the offset from start address
 * @params: start_addr start address of process
 * @params: curr_addr  curr_addr address of process
 * @return: offset
 */
ULONG findoffset(UINT proc_id, ULONG curr_addr) {

  proc_s *proc_obj = NULL;
  ULONG diff = 0;

  proc_obj = find_proc_obj(proc_id);
  if (proc_obj) {
    diff = curr_addr - (ULONG)proc_obj->start_addr;
    return diff;
  }
  return 0;
}


/* update offset for a mmapobj relative to start address
 * @params: proc_id
 * @params: vma_id
 * @params: offset
 * @return: 0 if success, -1 on failure
 */
int nv_record_chunk(rqst_s *rqst, ULONG addr) {

  proc_s *proc_obj;
  mmapobj_s *mmapobj = NULL;
  chunkobj_s *chunk = NULL;
  long vma_id = 0;
  UINT proc_id =0, offset =0;
  ULONG start_addr=0;
  rqst_s lcl_rqst;

  assert(rqst);

  //229360ms for 100000 keys
  /*check if the process is already in memory*/
  proc_id = rqst->pid;
  proc_obj = find_proc_obj(proc_id);

  //229360ms for 100000 keys
  /*Try loading process from persistent memory*/
  if(!proc_obj){

    #ifdef _NVDEBUG
    fprintf(stdout,"trying to load process \n");
    #endif

    proc_obj = load_process(proc_id, 0 /*permission*/);
    /*if we cannot load a process, then create a new one*/
    if(!proc_obj){

      #ifdef _NVDEBUG
      fprintf(stdout,"creating new process \n");
      #endif

      proc_obj = ( proc_s *)create_new_process(rqst);
      #ifdef _USE_TRANSACTION
      assert(!initialize_logmgr(proc_obj->pid, 1));
      #endif

    }else{
      #ifdef _USE_TRANSACTION
      assert(!initialize_logmgr(proc_obj->pid, 0));
      #endif
    }
  }
  assert(proc_obj);

  if(use_map_cache) {
    vma_id = locate_mmapobj_node((void *)addr, rqst, &start_addr,&mmapobj);
  }else{
    vma_id = locate_mmapobj_node((void *)addr, rqst, &start_addr,NULL);
  }
  assert(vma_id);
  assert(start_addr);

  //260267ms for 100000 keys
  /*first we try to locate an exisiting obhect*/
  /*add a new mmapobj and update to process*/
  if(!mmapobj){
    mmapobj = find_mmapobj( vma_id, proc_obj );
  }
  lcl_rqst.pid = rqst->pid;
  lcl_rqst.id = vma_id;

  if(!mmapobj) {
    lcl_rqst.bytes =get_vma_size(vma_id);
    mmapobj = add_mmapobj_to_proc(proc_obj, &lcl_rqst, offset, start_addr);
    assert(mmapobj);
  }else{
    lcl_rqst.bytes =mmapobj->length;
  }
  assert(lcl_rqst.bytes);

  //318730 for 100000 keys
  #ifdef _USE_DISKMAP
  /*Note: this is in assumption that disk map
  is sequential. so offset makes sense
  else offset is useless 
  we need to indicate current position in 
  the map (i.e.) how much we use the mmap*/
  mmapobj->offset = rqst->bytes + mmapobj->offset; 
  #endif

  /*find the mmapobj using vma id
    if application has supplied request id, neglect*/
  if(rqst->var_name) {

    lcl_rqst.id = gen_id_from_str(rqst->var_name);

    #ifdef _NVDEBUG
    fprintf(stdout,"generated chunkid %u from "
        "variable %s\n",lcl_rqst.id, rqst->var_name);
    #endif
  }
  else{
    lcl_rqst.id = rqst->id;
  }
  assert(lcl_rqst.id);
  //318730 for 100000 keys
  /*check if chunk already exists
  This is an ambiguos situation
  we do not know if programmar made an error
  in naming or intentionally same chunk id
  is used*/  

  offset = addr - start_addr;
  prev_proc_id = rqst->pid;
  lcl_rqst.pid = rqst->pid;
  lcl_rqst.nv_ptr = (void *)addr;
  lcl_rqst.log_ptr = rqst->log_ptr;
  lcl_rqst.bytes = rqst->bytes;  
  lcl_rqst.offset = offset;
  lcl_rqst.logptr_sz = rqst->logptr_sz;  
  lcl_rqst.no_dram_flg =  rqst->no_dram_flg;

    #if 0 
  chunk = find_chunk(mmapobj, lcl_rqst.id);
  if(chunk) {

    #ifdef _NVDEBUG
    fprintf(stdout,"Chunk already exists %s id %u\n",
        rqst->var_name, lcl_rqst.id);
    #endif  

    //FIXME: What about garbage collecting old pointers
    //Garbage collection must be done
    update_chunkobj(&lcl_rqst, mmapobj,chunk);
  }else {
    chunk = add_chunk_to_mmapobj(mmapobj, proc_obj, &lcl_rqst);
    ASSERT_NOTZERO(chunk);
    #ifdef _NVDEBUG
    fprintf(stdout,"adding chunk %s id: %d of size %u:"
        "to vma_id %u\n",
        rqst->var_name,
        lcl_rqst.id,
        lcl_rqst.bytes,vma_id);
    #endif
  }
  #endif 

    #if 1
  #ifdef _USE_TRANSACTION
  #ifdef _USE_SHADOWCOPY
    #ifdef _USE_UNDO_LOG
      assert(chunk->nv_ptr);
      /*keep a cache of chunks for easy lookup*/
      record_chunks(chunk->nv_ptr,chunk);
    #else
      assert(chunk->log_ptr);
      record_chunks(chunk->log_ptr,chunk);
    #endif
  #endif
  //gt_spinlock_init(&chunk->chunk_lock);
  //add_record(chunk);
  #endif
  #endif
  
  return SUCCESS;
}


/*if not process with such ID is created then
we return 0, else number of mapped blocks */
int get_proc_num_maps(int pid) {

  proc_s *proc_obj = NULL;
  proc_obj = find_proc_obj(pid);
  if(!proc_obj) {
    fprintf(stdout,"process not created \n");
    return 0;
  }
  else {
    //also update the number of mmapobj blocks
    return proc_obj->num_mmapobjs;
  }
  return 0;
}


int  iterate_chunk_obj(rbtree_node n) {

  chunkobj_s *chunkobj = NULL;

  if (n == NULL) {
    return 0;
  }

  if (n->right != NULL) {
    iterate_chunk_obj(n->right);
  }

  chunkobj = (chunkobj_s *)n->value;
  if(chunkobj) {  
    fprintf(stdout,"chunkobj chunkid: %d addr %lu\n", 
        chunkobj->chunkid, (unsigned long)chunkobj);
    return 0;
  }

  if (n->left != NULL) {
    iterate_chunk_obj(n->left);
  }
  return 0;
}


int  iterate_mmap_obj(rbtree_node n) {

  mmapobj_s *t_mmapobj = NULL;

  if (n == NULL) {
    return 0;
  }
  if (n->right != NULL) {
    iterate_mmap_obj(n->right);
  }

  t_mmapobj = (mmapobj_s *)n->value;

  if(t_mmapobj) {
    if(t_mmapobj->chunkobj_tree) {
      iterate_chunk_obj(t_mmapobj->chunkobj_tree->root);
    }
  }

  if (n->left != NULL) {
    iterate_mmap_obj(n->left);
  }
  return 0;
}

proc_s* load_process(int pid, int perm) {

  proc_s *proc_obj = NULL;
  proc_s *nv_proc_obj = NULL;
  mmapobj_s *mmapobj, *nv_mmapobj;

  size_t bytes = 0;
  int fd = -1;
  void *map;
  int idx = 0;
  ULONG addr = 0;
  char file_name[256];
  int wrt_perm = 0;

  //register signal handler
  //initseghandling();
  nv_initialize();

  bzero(file_name,256);
  bytes = sizeof(proc_s);
  generate_file_name((char *) PROCMETADATA_PATH, pid, file_name);
  fd = open(file_name, O_RDWR);
  if (fd == -1) {
    file_error( file_name);
    return NULL;
  }
  map = (proc_s *) mmap(0, PROC_METADAT_SZ,
      PROT_NV_RW, MAP_SHARED, fd, 0);

  nv_proc_obj = (proc_s *) map;
  if (nv_proc_obj == MAP_FAILED) {
    close(fd);
    fprintf(stdout,"Error mmapobjping the file %s\n", file_name);
    return NULL;
  }

  /*Check if process has write permission to modify
   *metadata
   */
  wrt_perm = check_modify_access(perm);

  /*Start reading the mmapobjs
   *add the process to proc_obj tree
   *add initialize the mmapobj list if not
   */
  addr = (ULONG) nv_proc_obj;
  addr = addr + sizeof(proc_s);

#ifdef _NVDEBUG
    fprintf(stdout,"WRITE permission %d\n", wrt_perm);
#endif

  if(wrt_perm){
    proc_obj = nv_proc_obj;
  }else {
    proc_obj = (proc_s *)malloc(sizeof(proc_s));
    assert(proc_obj);
    proc_obj->pid = nv_proc_obj->pid;
    proc_obj->size = nv_proc_obj->size;
    proc_obj->num_mmapobjs = nv_proc_obj->num_mmapobjs;
    proc_obj->start_addr = 0;
#ifdef _USE_TRANSACTION
    proc_obj->haslog = nv_proc_obj->haslog;
#endif
  }
  proc_obj->mmapobj_initialized = 0;
  proc_obj->mmapobj_tree = 0;
  add_proc_obj(proc_obj);

  if (!proc_obj->mmapobj_initialized){
    initialize_mmapobj_tree(proc_obj);
  }
#ifdef _NVDEBUG
  fprintf(stdout,"proc_obj->pid %d \n", proc_obj->pid);
  fprintf(stdout,"proc_obj->size %lu \n",proc_obj->size);
  fprintf(stdout,"proc_obj->num_mmapobjs %d\n", proc_obj->num_mmapobjs);
  fprintf(stdout,"proc_obj->start_addr %lu\n", proc_obj->start_addr);
#endif

  //Read all the mmapobjs
  for (idx = 0; idx < proc_obj->num_mmapobjs; idx++) {

    nv_mmapobj = (mmapobj_s*) addr;
#ifdef _NVDEBUG
    print_mmapobj(nv_mmapobj);
#endif
    if(wrt_perm){
      mmapobj = nv_mmapobj;
    }else {
      mmapobj = (mmapobj_s *)malloc(sizeof(mmapobj_s));
      copy_mmapobj(mmapobj, nv_mmapobj);
    }
    add_mmapobj(mmapobj, proc_obj);
    if(restore_chunk_objs(mmapobj, perm)){
      fprintf(stdout, "failed restoration\n");
      goto error;
    }

#ifdef ENABLE_CHECKPOINT  
    if(mmapobj)  
      record_vmas(mmapobj->vma_id, mmapobj->length);
      //record_vma_ghash(mmapobj->vma_id, mmapobj->length);  
#endif
    addr = addr + sizeof(mmapobj_s);
#ifdef _NVDEBUG
    print_mmapobj(mmapobj);
    //print_tree(mmapobj->chunkobj_tree);
#endif
  }
  return proc_obj;
  error:
  return NULL;
}

//This function just maps the address space corresponding
//to process.
void* map_nvram_state(rqst_s *rqst) {

  void *nvmap = NULL;
  int fd = -1;    
  nvarg_s a;
  a.proc_id = rqst->pid;
  assert(a.proc_id);
  a.fd = -1;
  a.vma_id =rqst->id;
  a.pflags = 1;
  a.noPersist = 0;
#ifdef _NVDEBUG
  printf("nvarg.proc_id %d %d %d\n",a.proc_id, rqst->bytes, rqst->id);
#endif
  nvmap = mmap_wrap(0,rqst->bytes,PROT_NV_RW, PROT_ANON_PRIV,fd,0, &a);
  assert(nvmap);
  if (nvmap == MAP_FAILED) {
    close(fd);
    goto error;
  }
  return nvmap;
  error:
  return NULL;
}

//////////////////FUNCTIONS USED FOR RECOVERY/////////////////////////////////////////////////
chunkobj_s *find_from_ptrlst(void *addr) {

  size_t bytes = 0;
  std::map <void*, chunkobj_s *>::iterator ptritr;
  unsigned long ptr = (unsigned long)addr;
  unsigned long start, end;

  if(!addr)
    return NULL;

  for( ptritr= pntr_lst.begin(); ptritr!=pntr_lst.end(); ++ptritr){
    chunkobj_s * chunk = (chunkobj_s *)(*ptritr).second;
    bytes = chunk->length;
    start = (ULONG)(*ptritr).first;
    end = start + bytes;
    if( ptr >= start && ptr <= end) {
      return chunk;
    }
  }
  return NULL;
}


int create_chunk_record(void* addr, chunkobj_s *chunksrc) {

  chunkobj_s *chunkcpy = (chunkobj_s *)malloc(sizeof(chunkobj_s));
  copy_chunkoj(chunkcpy,chunksrc);

  assert(chunksrc->nv_ptr);
  //copy chunk, does not copy nvptr, as might be stale
  //in some case. so we explicity check for null and
  //if not copy
  chunkcpy->old_nv_ptr = chunksrc->nv_ptr;
  pntr_lst[addr] = chunkcpy;
  fprintf(stdout,"creating records %lu, chunk id %u\n", addr, chunkcpy->chunkid);
  return 0;
}

int delete_chunk_record(){

  std::map <void*, chunkobj_s *>::iterator ptritr;

  for( ptritr= pntr_lst.begin(); ptritr!=pntr_lst.end(); ++ptritr){
    chunkobj_s *chunk = (chunkobj_s *)(*ptritr).second;
    if(chunk)
      free(chunk);
  }
  return 0;
}


// FIXME: Extremely fickle code
//Lot of assumptions, example about the pointer
//list.
//FIXME: Error in getting chunk just using
//chunk id without even using process id
void *load_valid_addr(void **ptr) {

  chunkobj_s *oldchunk, *newchunk;
  ULONG offset=0;
  void *nv_ptr= NULL;

  //if(*ptr)
  //fprintf(stdout,"searching addr %lu\n", *ptr);

  if(ptr == NULL)
    return NULL;

  if(NULL == *ptr)
    return NULL;

  oldchunk = find_from_ptrlst(*ptr);
  if(!oldchunk){
    //could not find an equivalent pointer
    return NULL;
  }
  //Ok we found a chunk corresponding to
  //this pointed. Now lets find, in what
  //offset of chunk was this pointed located
  offset = (ULONG)*ptr - (ULONG)oldchunk->old_nv_ptr;

  //Now lets find the current NV_PTR for this
  //chunk
  rqst_s rqst;
  rqst.pid = oldchunk->pid;
  rqst.id = oldchunk->chunkid;
  rqst.var_name = NULL;

  nv_ptr = nv_map_read(&rqst, NULL /*always NULL*/);
  //All old states must have been loaded
  assert(nv_ptr);
  *ptr = nv_ptr + offset;

  fprintf(stdout,"oldchunk->chunkid %d\n", oldchunk->chunkid);

  //fprintf(stdout,"loaded start addr %lu, *ptr %lu, offset %lu\n", (unsigned long)nv_ptr, *ptr,offset);
  return *ptr;
}


//////////////////END FUNCTIONS USED FOR RECOVERY/////////////////////////////////////////////////

#if 0
void* nv_map_read(rqst_s *rqst, void* map ) {

  ULONG offset = 0;
  ULONG addr_l = 0;
  int process_id = 1;
  proc_s *proc_obj = NULL;
  unsigned int chunk_id;
  mmapobj_s *mmapobj_ptr = NULL;
  void *map_read = NULL;
  rbtree_t *tree_ptr;
  chunkobj_s *chunkobj;
  int perm = rqst->access;
  char *del;

#ifdef _USE_DISKMAP
  char out_fname[256];
  FILE *fp = NULL;
  int fd = -1;
  struct stat statbuf;
#endif

#ifdef _VALIDATE_CHKSM
  char gen_key[256];
  long hash;
#endif

  process_id = rqst->pid;
  //Check if all the process objects are still in memory and we are not reading
  //process for the first time
  proc_obj = find_process(rqst->pid);
  if(!proc_obj) {
    /*looks like we are reading persistent structures
    and the process is not avaialable in memory
      FIXME: this just addressies one process,
    since map_read field is global*/

#ifdef _NVDEBUG
    printf("invoking load_process \n");
#endif

    proc_obj = load_process(process_id, perm);
    if(!proc_obj){
#ifdef _NVDEBUG
      printf("proc object for %d failed\n", process_id);
#endif
      goto error;
    }
  }
#ifdef _NVDEBUG
  printf("finished loading process \n");


#endif

  if(rqst->var_name){
    chunk_id = gen_id_from_str(rqst->var_name);
  }
  else{
    chunk_id = rqst->id;
  }

  mmapobj_ptr = find_mmapobj_from_chunkid( chunk_id, proc_obj );
  if(!mmapobj_ptr) {
    fprintf(stdout,"finding mmapobj %u for proc"
        "%d failed \n",chunk_id, process_id);
    goto error;
  }

  rqst->id = mmapobj_ptr->vma_id;
  rqst->pid =mmapobj_ptr->proc_id;
  rqst->bytes= mmapobj_ptr->length;
  map_read = map_nvram_state(rqst);
  if(!map_read){
    fprintf(stdout, "nv_map_read:"
        " map_process returned null \n");
    goto error;
  }

  //Get the the start address and then end address of mmapobj
  /*Every malloc call will lead to a mmapobj creation*/
  tree_ptr = mmapobj_ptr->chunkobj_tree;
  chunkobj  = (chunkobj_s *)rbtree_lookup(tree_ptr,
      (void*)chunk_id, IntComp);
  assert(chunkobj);
#ifdef _NVDEBUG
  print_chunkobj(chunkobj);
#endif

  offset = chunkobj->offset;
  addr_l = (ULONG)map_read+ offset;
  chunkobj->nv_ptr = (void *)addr_l;

  //update start addr
  mmapobj_ptr->data_addr = (ULONG)map_read;

#ifdef _USE_SHADOWCOPY
  chunkobj->log_ptr = malloc(chunkobj->length);


  assert(chunkobj->log_ptr);
  memcpy(chunkobj->log_ptr, chunkobj->nv_ptr, chunkobj->length);
#endif

#ifdef _VALIDATE_CHKSM
  bzero(gen_key, 256);
  sha1_mykeygen(chunkobj->log_ptr, gen_key,
      CHKSUM_LEN, 16, chunkobj->length);
  hash = gen_id_from_str(gen_key);
  if(hash != chunkobj->checksum){
    //fprintf(stdout,"CHUNK CORRUPTION \n");
    print_chunkobj(chunkobj);
    //goto error;
  }else {

  }
#endif

  rqst->nv_ptr = chunkobj->nv_ptr;
#ifdef _USE_SHADOWCOPY
  rqst->log_ptr= chunkobj->log_ptr;
#endif
  rqst->bytes = chunkobj->length;

#ifdef _NVDEBUG
  fprintf(stdout, "nv_map_read: mmapobj offset"
      "%lu %lu %u \n", offset,
      (ULONG)map_read,
      mmapobj_ptr->vma_id);
#endif

#ifdef _NVSTATS
  add_stats_chunk_read(rqst->pid, chunkobj->length);
  print_stats(rqst->pid);
#endif

  return (void *)rqst->nv_ptr;

  error:
  fprintf(stdout,"read failed \n");
  rqst->nv_ptr = NULL;
  rqst->log_ptr = NULL;
  return NULL;
}
#endif

void* nv_map_read(rqst_s *rqst, void* map ) {

  ULONG offset = 0;
  ULONG addr_l = 0;
  int process_id = 1;
  proc_s *proc_obj = NULL;
  unsigned int chunk_id;
  mmapobj_s *mmapobj_ptr = NULL;
  void *map_read = NULL;
  rbtree_t *tree_ptr;
  chunkobj_s *chunkobj;
  int perm = rqst->access;
  char *del;

#ifdef _USE_DISKMAP
  char out_fname[256];
  FILE *fp = NULL;
  int fd = -1;
  struct stat statbuf;
#endif

#ifdef _VALIDATE_CHKSM
  char gen_key[256];
  long hash;
#endif
  process_id = rqst->pid;
  //Check if all the process objects are still in memory and we are not reading
  //process for the first time
  proc_obj = find_process(rqst->pid);
  if(!proc_obj) {
    /*looks like we are reading persistent structures
    and the process is not avaialable in memory
      FIXME: this just addressies one process,
    since map_read field is global*/

#ifdef _NVDEBUG
    printf("invoking load_process \n");
#endif

    proc_obj = load_process(process_id, perm);
    if(!proc_obj){
#ifdef _NVDEBUG
      printf("proc object for %d failed\n", process_id);
#endif
      goto error;
    }
  }
#ifdef _NVDEBUG
  printf("finished loading process \n");
#endif
  if(rqst->var_name){
    chunk_id = gen_id_from_str(rqst->var_name);
  }
  else{
    chunk_id = rqst->id;
  }

  mmapobj_ptr = find_mmapobj_from_chunkid( chunk_id, proc_obj );
  if(!mmapobj_ptr) {
    //fprintf(stdout,"finding mmapobj %u for proc"
    //    "%d failed \n",chunk_id, process_id);
    goto error;        
  }

  //Get the the start address and then end address of mmapobj
  /*Every malloc call will lead to a mmapobj creation*/
  tree_ptr = mmapobj_ptr->chunkobj_tree;  
  chunkobj  = (chunkobj_s *)rbtree_lookup(tree_ptr,
      (void*)chunk_id, IntComp);

  assert(chunkobj);
  assert(chunkobj->nv_ptr);
  assert(chunkobj->offset);

#ifdef _USE_SHADOWCOPY
  chunkobj->log_ptr = malloc(chunkobj->length);
  assert(chunkobj->log_ptr);
  memcpy(chunkobj->log_ptr, chunkobj->nv_ptr, chunkobj->length);
#endif

#ifdef _VALIDATE_CHKSM
  bzero(gen_key, 256);
  sha1_mykeygen(chunkobj->log_ptr, gen_key,
      CHKSUM_LEN, 16, chunkobj->length);
  hash = gen_id_from_str(gen_key);
  if(hash != chunkobj->checksum){
    //fprintf(stdout,"CHUNK CORRUPTION \n");
    print_chunkobj(chunkobj);  
    //goto error;
  }else {

  }
#endif
  rqst->nv_ptr = chunkobj->nv_ptr;
  rqst->bytes = chunkobj->length;

  #ifdef _USE_SHADOWCOPY
  rqst->log_ptr= chunkobj->log_ptr;
#endif

#ifdef _NVDEBUG
  fprintf(stdout, "nv_map_read: mmapobj offset"
      "%lu %lu %u \n", offset,
      (ULONG)map_read,
      mmapobj_ptr->vma_id);
#endif

#ifdef _NVSTATS
  add_stats_chunk_read(rqst->pid, chunkobj->length);
  print_stats(rqst->pid);
#endif

  return (void *)rqst->nv_ptr;

error:
//  /fprintf(stdout,"read failed \n");
  rqst->nv_ptr = NULL;
  rqst->log_ptr = NULL;
  return NULL;
}


int nv_munmap(void *addr){

  int ret_val = 0;

  if(!addr) {
    perror("null address \n");
    return -1;
  }
  ret_val = munmap(addr, MAX_DATA_SIZE);
  return ret_val;
}




void *create_map_tree() {

  if(!map_tree)
    map_tree =rbtree_create();

  if(!map_tree){
    perror("RB tree creation failed \n");
    exit(-1);
  }
  return map_tree;
}

int insert_mmapobj_node(ULONG val, size_t size, int id, int proc_id) {

  struct mmapobj_nodes *mmapobj_struct;
  //rb_red_blk_node *node = NULL;
  mmapobj_struct = (struct mmapobj_nodes*)malloc(sizeof(struct mmapobj_nodes));
  mmapobj_struct->start_addr = val;
  mmapobj_struct->end_addr = val + size;
  mmapobj_struct->map_id = id;
  mmapobj_struct->proc_id= proc_id;

  if(!map_tree)
    create_map_tree();
  #ifdef _NVDEBUG
  fprintf(stdout,"before insert mapid %u start_addr %lu "
      "end_addr %lu, proc_id %d  map_tree %lu \n",
      mmapobj_struct->map_id,
      mmapobj_struct->start_addr,
      mmapobj_struct->end_addr,
      mmapobj_struct->proc_id, (ULONG)map_tree);
  #endif
  rbtree_insert(map_tree,(void*)val,mmapobj_struct, IntComp);
  return 0;
}

UINT locate_mmapobj_node(void *addr, rqst_s *rqst, 
              ULONG *map_strt_addr, 
              mmapobj_s **mmap_obj){

  struct mmapobj_nodes *mmapobj_struct = NULL;
  ULONG addr_long, strt_addr;
  UINT mapid;

  if(use_map_cache){
    mmapobj_s *tmpobj;
    tmpobj =get_frm_mmap_cache(addr);  
    if(tmpobj){
      *map_strt_addr = tmpobj->data_addr;
      assert(mmap_obj);
      *mmap_obj= tmpobj; 
      return tmpobj->vma_id;
    }
  }

  if(mmap_obj)
      *mmap_obj= NULL;
  
  addr_long = (ULONG)addr;
  mmapobj_struct = (struct mmapobj_nodes *)rbtree_lookup(map_tree,(void *)addr_long,
      CompRange);
  if(mmapobj_struct) {
    mapid = mmapobj_struct->map_id;
    strt_addr = mmapobj_struct->start_addr;

    #ifdef _NVDEBUG_L2
    fprintf(stdout,"addr: %lu, query start:%lu, end %lu mapid %d"
        "map_tree %lu\n",
        (ULONG)addr,strt_addr,
        mmapobj_struct->end_addr,
        mapid, (ULONG)map_tree);
    #endif
    *map_strt_addr= strt_addr;
    return mapid;
  }
  #ifdef _NVDEBUG
  fprintf(stdout,"query failed pid:%d %u addr: %lu\n",
      rqst->pid, rqst->id, addr_long);
  #endif
  return 0;
}




int id = 0;
size_t total_size =0;
void* _mmap(void *addr, size_t size, int mode, int prot, 
    int fd, int offset, nvarg_s *a){

  void *ret = NULL;
  ULONG addr_long=0;

  assert(a);
  assert(a->proc_id);
  a->fd = -1;
  a->vma_id = ++id;
  a->pflags = 1;
  a->noPersist = 0;
  total_size += size;

  ret = mmap_wrap(addr,size, mode, prot,fd,offset, a); 
  assert(ret);

#ifdef _NVSTATS
  total_mmaps++;
  add_stats_mmap(a->proc_id, size);
#endif

  addr_long = (ULONG)ret;
  insert_mmapobj_node(addr_long, size, id, a->proc_id);      

  record_vmas(a->vma_id, size);
  //record_vma_ghash(a->vma_id, size);

  return ret;
}

#ifdef ENABLE_CHECKPOINT


int reg_for_signal(int procid) {

  /*termination signal*/
  app_exec_finish_sig(procid, SIGUSR2);

  return register_ckpt_lock_sig(procid, SIGUSR1);
}


int init_checkpoint(int procid) {

  init_shm_lock(procid);

  //pthread_mutex_init(&chkpt_mutex, NULL);
  //pthread_mutex_lock(&chkpt_mutex);
  //mutex_set = 1;
  return 0;
}


int  chkpt_all_chunks(rbtree_node n, int *cmt_chunks) {

  int ret =-1;
  chunkobj_s *chunkobj = NULL;
  char gen_key[256];
  ULONG lat_ns, cycles;


#ifdef _VALIDATE_CHKSM  
  long hash;
#endif

  if (n == NULL) {
    return 0;
  }

  if (n->right != NULL) {
    ret = chkpt_all_chunks(n->right,cmt_chunks);
  }

  chunkobj = (chunkobj_s *)n->value;

  if(chunkobj) {  

#ifdef _ASYNC_LCL_CHK
    if(chunkobj->dirty) {
#endif
      void *src = chunkobj->log_ptr;
      void *dest = chunkobj->nv_ptr;

      assert(src);
      assert(dest);
      assert(chunkobj->length);

      //#ifdef _NVDEBUG
      if(prev_proc_id == 1)
        fprintf(stdout,"commiting chk no:%d chunk %u "
            "and size %u \t"
            "committed? %d \n",
            local_chkpt_cnt,
            chunkobj->chunkid,
            chunkobj->length,
            chunkobj->dirty);
      //#endif
      *cmt_chunks = *cmt_chunks + 1;

      //if(prev_proc_id == 1)
      //print_stats(prev_proc_id);

#ifdef _ASYNC_LCL_CHK
      chunkobj->dirty = 0;
#endif

#ifdef _ASYNC_RMT_CHKPT
      chunkobj->rmt_nvdirtchunk = 1;
#endif

#ifdef _COMPARE_PAGES 
      compare_content_hash(src,dest, chunkobj->length);
#endif
      size_t cmpr_len = 0;
      //if(prev_proc_id == 1)
      memcpy_delay(dest,src,chunkobj->length);
      //snappy::RawCompress((const char *)src, chunkobj->length, (char *)dest, &cmpr_len);
      //fprintf(stdout,"Before %u After Compre %u \n", chunkobj->length, cmpr_len);

#ifdef _NVSTATS
      proc_stat.tot_cmtdata += chunkobj->length;
      add_to_chunk_memcpy(chunkobj);
#endif      

#ifdef _VALIDATE_CHKSM
      bzero(gen_key, 256);
      sha1_mykeygen(src, gen_key,
          CHKSUM_LEN, 16, chunkobj->length);

      hash = gen_id_from_str(gen_key);

      chunkobj->checksum = hash;
#endif
      ret = 0;

#ifdef _ASYNC_LCL_CHK
    }
#endif

  }

  if (n->left != NULL) {
    return chkpt_all_chunks(n->left,cmt_chunks);
  }
  return ret;
}

int  chkpt_all_vmas(rbtree_node n) {

  int ret =-1;
  mmapobj_s *t_mmapobj = NULL;
  rbtree_node root;
  int cmt_chunks =0, tot_chunks=0;

  //assert(n);
  if (n == NULL) {
    return 0;
  }

  if (n->right != NULL) {
    ret = chkpt_all_vmas(n->right);
  }

  t_mmapobj = (mmapobj_s *)n->value;

  if(t_mmapobj) {
    if(t_mmapobj->chunkobj_tree) {

#ifdef _NVDEBUG
      print_mmapobj(t_mmapobj);
#endif
      root = t_mmapobj->chunkobj_tree->root;
      if(root)

#ifdef _USE_GPU_CHECKPT
        ret = gpu_chkpt_all_chunks(root, &cmt_chunks);
#else
      ret = chkpt_all_chunks(root, &cmt_chunks);
#endif
    }
  }

  if (n->left != NULL) {
    return chkpt_all_vmas(n->left);
  }

  tot_chunks = get_chnk_cnt_frm_map();

#ifdef _NVDEBUG
  fprintf(stdout,"total chunks %d, cmt chunks %d\n",
      tot_chunks, cmt_chunks);
#endif

  return ret;
}


int nv_chkpt_all(rqst_s *rqst, int remoteckpt) {

  int process_id = -1;
  proc_s *proc_obj= NULL;   
  rbtree_node root;
  int ret = 0;

  local_chkpt_cnt++;

#ifdef _NVSTATS
  long cmttime =0; 
  struct timeval ckpt_start_time, ckpt_end_time;

  gettimeofday(&ckpt_start_time, NULL);
#endif

  if(!rqst)
    goto error;

  process_id = rqst->pid;
  proc_obj= find_proc_obj(process_id);

#ifdef _NVDEBUG
  fprintf(stdout,"invoking commit "
      "for process %d \n",
      rqst->pid);

  assert(proc_obj);
  assert(proc_obj->mmapobj_tree);
  assert(proc_obj->mmapobj_tree->root);
#endif
  if(!proc_obj) 
    goto error;

  if(!(proc_obj->mmapobj_tree))
    goto error;

  root = proc_obj->mmapobj_tree->root;
  if(!root) 
    goto error;

#ifdef _NVSTATS
  gettimeofday(&commit_end, NULL);
  cmttime = simulation_time( commit_start,commit_end);

  chkpt_itr_time = cmttime;

  add_stats_commit_freq(process_id, cmttime);  
#endif

  //set_acquire_chkpt_lock(process_id);
  ret = chkpt_all_vmas(root);
#ifdef _ASYNC_RMT_CHKPT
  set_ckptdirtflg(process_id);
#endif
  //if(prev_proc_id == 1)  
  //print_stats(process_id);

  //disable_chkpt_lock(process_id);
  //
  incr_lcl_chkpt_cnt();

#ifdef _COMPARE_PAGES 
  hash_clear();
#endif

#ifdef _FAULT_STATS
  if(prev_proc_id == 1)
    set_chunkprot();
#endif //_FAULT_STATS


#ifdef _ASYNC_LCL_CHK
  set_chunkprot();
  /*if(checpt_cnt ==1)
    stop_history_coll = 1;
  clear_fault_lst();
  checpt_cnt++;*/
#endif

  error:

#ifdef _ASYNC_RMT_CHKPT
  set_chkpt_type(SYNC_LCL_CKPT);
  send_lock_avbl_sig(SIGUSR1);
#else
  pthread_cond_signal(&dataPresentCondition);
  //pthread_mutex_unlock(&chkpt_mutex);
#endif


#ifdef _USE_FAULT_PATTERNS
  if(!check_chunk_fault_lst_empty())
    chunk_fault_lst_freeze =1;
  //if(prev_proc_id == 1)
  //  print_chunk_fault_lst();
#endif


#ifdef _NVSTATS
  gettimeofday(&ckpt_end_time, NULL);

  add_stats_chkpt_time(process_id,
      simulation_time(ckpt_start_time,ckpt_end_time));

  gettimeofday(&commit_start, NULL);
#endif

  if(!ret){
#ifdef _NVDEBUG
    printf("nv_chkpt_all: succeeded for procid"
        " %d \n",proc_obj->pid); 
#endif
    return ret;
  }

#ifdef _NVDEBUG
  printf("nv_chkpt_all: failed for procid"
      " %d \n",proc_obj->pid); 
#endif
  return -1;
}
#endif



/*Called by nv_commit, or undo log
 * takes rqst as input, with mandatatory
 * process id, and varname/chunkid as param
 */
chunkobj_s * get_chunk(rqst_s *rqst) {

  int process_id = -1;
  int ops = -1;
  proc_s *proc_obj= NULL;   
  unsigned int chunkid = 0;
  mmapobj_s *mmapobj_ptr= NULL;

  if(!rqst)
    return NULL;

  //we don't need size if transaction,
  //we just commit the entire object
#ifndef _USE_TRANSACTION
  assert(rqst->bytes);
#endif
  process_id = rqst->pid;
  assert(process_id);

#ifndef _USE_TRANSACTION
  ops = rqst->ops;
#endif
  proc_obj= find_proc_obj(process_id);
  assert(proc_obj);

  /*find the mmapobj if application has
   supplied request id, neglect
   the varname. in case of transactions applications
   can just supply the transaction ID*/
  if(!rqst->id) {
    if(rqst->var_name)
      chunkid = gen_id_from_str(rqst->var_name);
    else
      printf("nv_commit:error generating vma id \n");
  }
  else{
    chunkid = rqst->id;
  }
  /*we always make assumption the
    chunk id is always greater than 1*/
  assert(chunkid);
  //we verify if such a chunk exists
  mmapobj_ptr = find_mmapobj_from_chunkid( chunkid, proc_obj );
  assert(mmapobj_ptr);
#ifdef CHCKPT_HPC
  mmapobj_ptr->ops = ops;
  mmapobj_ptr->order_id = rqst->order_id;
#endif

  return find_chunk(mmapobj_ptr, chunkid);
}

int nv_commit(rqst_s *rqst) {

  chunkobj_s *chunk=NULL;
  void* dest = NULL;
  void *src = NULL;

  chunk =  get_chunk(rqst);
  assert(chunk);
  assert(chunk->length);

#ifdef _USE_SHADOWCOPY
  src = chunk->log_ptr;
  dest =chunk->nv_ptr;
#else
  src = chunk->nv_ptr;
  dest = chunk->nv_ptr;
#endif
  assert(src);
  assert(dest);
  memcpy(dest, src, chunk->length);
  chunk->dirty=1;
#ifdef _NVDEBUG
  fprintf(stderr,"nv_commit: COMPLETE \n");
#endif
  return 0;
}

int app_exec_finish(int pid){

  #ifdef _USE_DISKMAP
    disk_flush(pid);
  #endif

  #ifdef _ASYNC_RMT_CHKPT
    send_lock_avbl_sig(SIGUSR2);
  #endif

  #ifdef _NVSTATS
    if(pid == 1)
        print_stats(pid);
  #endif

    #ifdef _USE_TRANSACTION
  print_trans_stats();
  #endif

    return 0;
}

#ifdef _NVSTATS

int clear_start(int pid) {
  proc_stat.tot_cmtdata = 0;
}

int add_stats_chunk(int pid, size_t chunksize) {

  proc_stat.pid = pid;
  proc_stat.chunk_dist[proc_stat.num_chunks] = chunksize;
  proc_stat.tot_chunksz += chunksize;
  proc_stat.num_chunks++;
  return 0;
}

int add_stats_chunk_read(int pid, size_t chunksize) {

  proc_stat.pid = pid;
  proc_stat.tot_rd_chunksz += chunksize;
  proc_stat.num_rd_chunks++;
  return 0;
}

int add_stats_mmap(int pid, size_t mmap_size) {

  proc_stat.pid = pid;
  proc_stat.num_mmaps++;
  proc_stat.tot_mmapsz += mmap_size;
}


int add_stats_commit_freq(int pid, long time) {

  proc_stat.pid = pid;
  proc_stat.commit_freq = time;
}

int add_stats_chkpt_time(int pid, long time) {

  proc_stat.pid = pid;
  proc_stat.per_step_chkpt_time = time;
}

int add_to_chunk_memcpy(chunkobj_s *chunk) {

  if(chunk);
  chunk->num_memcpy = chunk->num_memcpy +1;
}



int print_stats(int pid) {

  int i = 0;

  fprintf(stdout,"*************************\n");
  fprintf(stdout, "PID: %d \n",proc_stat.pid);
  fprintf(stdout, "NUM MMAPS: %d \n",proc_stat.num_mmaps);
  fprintf(stdout, "NUM CHUNKS: %d \n",proc_stat.num_chunks);
  fprintf(stdout, "MAPSIZE: %u \n",proc_stat.tot_mmapsz);
  fprintf(stdout, "CHUNKSIZE: %u \n",proc_stat.tot_chunksz);
#ifdef _USE_CHECKPOINT
  fprintf(stdout, "CHKPT COUNT: %u \n",local_chkpt_cnt);
  fprintf(stdout, "TOT COMMIT SIZE: %u \n",proc_stat.tot_cmtdata);
  fprintf(stdout, "COMMIT FREQ %ld \n",proc_stat.commit_freq);
  fprintf(stdout, "TOT CHKPT TIME %ld \n",proc_stat.per_step_chkpt_time);
#endif
  fprintf(stdout, "TOT READ CHUNKSZ %u \n",proc_stat.tot_rd_chunksz);
  fprintf(stdout, "NUM READ CHUNKs COUNT %d \n",proc_stat.num_rd_chunks);
  fprintf(stdout, "CHUNKSIZE STAT: \t");

#ifdef _USE_CHECKPOINT
  for( i =0; i< proc_stat.num_chunks; i++){
    fprintf(stdout,"%u ",proc_stat.chunk_dist[i]);
  }

  print_all_chunks();
  print_adtnl_copy_overhead(local_chkpt_cnt);
  fprintf(stdout,"\n\n");
#endif
  /*reset incremeneting fields */
  clear_start(proc_stat.pid);
}

#endif


#ifdef _NVRAM_OPTIMIZE
void add_to_mmap_cache(mmapobj_s *mmapobj) {

   mmap_strt_addr_cache[next_mmap_entry]= mmapobj->data_addr;
   mmap_size_cache[next_mmap_entry]= mmapobj->length;
   mmap_ref_cache[next_mmap_entry] = (ULONG)mmapobj;
   //fprintf(stdout,"startaddr %lu next_mmap_entry %lu\n",mmapobj->strt_addr, next_mmap_entry);
   next_mmap_entry++;
   next_mmap_entry = next_mmap_entry % NUM_MMAP_CACHE_CNT;
}

mmapobj_s* get_frm_mmap_cache(void *addr) {

  UINT idx =0;
  ULONG target = (ULONG)addr;

  for (idx =0; idx < NUM_MMAP_CACHE_CNT; idx++) {

    ULONG startaddr = mmap_strt_addr_cache[idx];

     if (startaddr <= target && 
        startaddr + mmap_size_cache[idx] > target)
     {
      return (mmapobj_s*) mmap_ref_cache[idx];
      }else {
      //fprintf(stdout,"miss \n");              
      }
   }
   return NULL;
}

#endif

