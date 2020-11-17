/******************************************************************************
 * FILE: mergesort.c
 * DESCRIPTION:
 *   The master task distributes an array to the workers in chunks, zero pads for equal load balancing
 *   The workers sort and return to the master, which does a final merge
 ******************************************************************************/
//#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <math.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>
#include <sys/queue.h>
#include <sys/resource.h>

#include "jemalloc/jemalloc.h"
#include "nv_map.h"
#include "error_codes.h"
#include "nv_def.h"
#include "allocs/np_malloc.h"
//#include "nv_rmtckpt.h"
#include "util_func.h"
#include "nv_transact.h"
//#include "checkpoint.h"
#include "hash_maps.h"

#include "malloc_hook.h"

//#define _CHKPTIF_DEBUG
//#define _NONVMEM
#define _NVMEM
//#define IO_FORWARDING
//#define LOCAL_PROCESSING
#define FREQUENCY 1
//#define FREQ_MEASURE

#ifdef _USE_CMU_NVMALLOC
#include "nvmalloc.h"
static unsigned int cmu_nvallocinit=0;
#define MAX_CMU_PAGES 50600 //1 GB pages
#endif

static unsigned int BASEPROCESSID; //=0;

/*void showdata(double *v, int n, int id);
double * merge(double *A, int asize, double *B, int bsize);
void swap(double *v, int i, int j);
void m_sort(double *A, int min, int max);
extern void *run_rmt_checkpoint(void *args); */

long simulationtime(struct timeval start,
    struct timeval end );

int thread_init = 0, set_affinity=0;
double startT, stopT;
double startTime;
unsigned long total_bytes=0;
int iter_count =0;
struct timeval g_start, g_end;
struct timeval g_chkpt_inter_strt, g_chkpt_inter_end;
int g_mypid=0;
size_t totalsize;
u_int8_t init_chkpt;


#ifdef FREQ_MEASURE
//For measurement
double g_iofreq_time=0;
#endif

pthread_mutex_t precommit_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t precommit_cond = PTHREAD_COND_INITIALIZER;
int precommit=0;
int curr_chunkid =0;
int prev_chunkid =0;

pthread_mutex_t alloc_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dealloc_mtx = PTHREAD_MUTEX_INITIALIZER;

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

#ifdef _USE_CHECKPOINT

static void
handler(int sig, siginfo_t *si, void *unused)
{

  /*struct sigaction sa;
   struct sched_param param;
    sa.sa_flags = SA_SIGINFO;
   sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
         handle_error("sigaction");*/


  //fprintf(stdout,"recvd seg fault \n");
  size_t length = nv_disablprot(si->si_addr, &curr_chunkid);
  assert(length > 0);

  if(prev_chunkid == 0) {
    prev_chunkid = curr_chunkid;
  }else {

    if(prev_chunkid)
      add_to_fault_lst(prev_chunkid);
    //pthread_mutex_unlock(&precommit_mtx);a
    precommit=1;
    prev_chunkid = curr_chunkid;
    pthread_cond_signal(&precommit_cond);
#ifdef _CHKPTIF_DEBUG
    fprintf(stdout, "sent message to async thread to start"
        " async lckpt thread...\n");
#endif
  }
}


int assign_aff() {

  int core_id = 11;

  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (core_id >= num_cores)
    return -1;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  int return_val = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

}

int exclude_aff(int cpu) {

  int core_id = cpu;
  int j=0;

  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

  if (core_id >= num_cores)
    return -1;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (j = 0; j < CPU_SETSIZE; j++)
    if( j != cpu)
      CPU_SET(j, &cpuset);

  pthread_t current_thread = pthread_self();
  return  pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}


int get_aff() {

  int  j =0;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

  for (j = 0; j < num_cores; j++)
    CPU_SET(j, &cpuset);

  pthread_t current_thread = pthread_self();
  int ret_val = pthread_getaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
  for (j = 0; j < CPU_SETSIZE; j++)
    if (CPU_ISSET(j, &cpuset))
      fprintf(stdout,"thread affinity of async thread %d is %d \n", g_mypid, j);
  return j;
}




int mysleep(int time) {
  struct timeval tv;
  tv.tv_sec =time;
  tv.tv_usec = 0;
  select(0, NULL, NULL, NULL, &tv);
  return 0;
}  



void* set_protection(void *arg)
{
  long itr;
  long simtime;
  long sleep_time;
  int target_chunk =0;

  assign_aff();
  while(1) {

#ifdef _CHKPTIF_DEBUG
    fprintf(stdout, "starting async lcl ckpt thread...\n");
#endif
    pthread_mutex_lock(&precommit_mtx);

    while(!precommit)
      pthread_cond_wait(&precommit_cond, &precommit_mtx);

    itr = get_chkpt_itr_time();
    chkpt_itr_time = itr;
    if(itr > 100000000)
      goto exit;


#ifdef RMT_PRECOPY
    //send_lock_avbl_sig(SIGUSR1);
#endif

    gettimeofday(&g_chkpt_inter_end, NULL);
    simtime = simulationtime(g_chkpt_inter_strt, g_chkpt_inter_end);

    if( simtime < chkpt_itr_time) { // THRES_ASYNC){
      sleep_time =  chkpt_itr_time - simtime; //THRES_ASYNC - simtime;
      sleep_time = (sleep_time/1000000);

      //if(g_mypid == 3)
      //fprintf(stdout,"SLEEP TIME %ld \n",sleep_time);
      //mysleep(sleep_time);
      sleep(sleep_time);
    }
#ifdef _CHKPTIF_DEBUG
    fprintf(stdout,"async lc thread: I am starting async chkpt \n");
#endif

    start_asyn_lcl_chkpt(target_chunk);

    exit:
    precommit=0;
    pthread_mutex_unlock(&precommit_mtx);
  }
  return 0;
}


int start_precommit_() {

  //precommit=1;
  //pthread_cond_signal(&precommit_cond);
  return 0;
}



#ifdef _ASYNC_LCL_CHK

int generate_priority(pthread_attr_t *lp_attr,  struct sched_param *param){

  int min_priority;
  pthread_attr_init(lp_attr);
  pthread_attr_setinheritsched(lp_attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(lp_attr, SCHED_FIFO);
  min_priority =  sched_get_priority_min(SCHED_FIFO); //15
  //  param.sched_priority = min_priority;
  pthread_attr_setschedparam(lp_attr, param);
}

void start_async_commit()
{
  pthread_t thread1;
  int  iret1;
  struct sigaction sa;
  struct sched_param param;
  pthread_attr_t lp_attr;
  int s =0;  
  int policy, min_priority;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = handler;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    handle_error("sigaction");

  //generate_priority(&lp_attr,  &param);
  //iret1 = pthread_create(&thread1,&lp_attr, set_protection, (void*)NULL);

  /* Create independent threads each of which will execute function */
  iret1 = pthread_create(&thread1,NULL, set_protection, (void*)NULL);
}

#endif


#endif

/* To calculate simulation time */
long simulationtime(struct timeval start, struct timeval end )
{
  long current_time;

  current_time = ((end.tv_sec * 1000000 + end.tv_usec) -
      (start.tv_sec*1000000 + start.tv_usec));

  return current_time;
}

int starttime_(int *mype) {

  //if(*mype == 0)
  {
    gettimeofday(&g_start, NULL);
  }
  return 0;
}


int endtime_(int *mype, float *itr) {

  //if(*mype == 0)
  {
    gettimeofday(&g_end, NULL);
    fprintf(stderr,"END TIME: %ld mype %d \n ",
        simulationtime(g_start, g_end),(int)*mype);
  }
  return 0;
}


unsigned int BASEID_GET(){

   BASEPROCESSID=getpid();
   return BASEPROCESSID;

#if 0
  if(!BASEPROCID){
#ifdef _USERANDOM_PROCID

    struct timeval currtime;

    gettimeofday(&currtime, NULL);

    BASEPROCID= gen_rand(currtime.tv_usec % 533333, (
        currtime.tv_usec+1) % 543333);

    printf("BASEPROCID %u\n",BASEPROCID);
#else
    BASEPROCID = 600;
#endif
  }
  //printf("BASEPROCID %u\n",BASEPROCID);
  return BASEPROCID;
#endif
}
extern "C" {

int nvinit(UINT pid) {

  //my_init_hook();
  //con();
  pid = BASEID_GET() + 1;
  //return load_process(pid, 1);
  return nv_initialize(pid);
}

int c_app_stop_(int pid) {

  pid =  BASEID_GET() + 1;
  return  app_exec_finish(pid);
}

}

int nvinit_(UINT pid) {
   return nvinit(pid);
}

int app_stop_(int pid){

  return c_app_stop_(pid);
}


void *nv_jemalloc(size_t size, rqst_s *rqst) {

  return je_malloc_((size_t)size, rqst);
}

void* nvread_(char *var, int id)
{
  void *buffer = NULL;
  rqst_s rqst;
  size_t len=0;

  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.id = 0;
  rqst.access = 0;

  len = strlen(var);
  memcpy(rqst.var_name,var,len);
  rqst.var_name[len] = 0;
  rqst.no_dram_flg = 1;
#ifdef _USE_BASIC_MMAP
  len=0;
  return read_mmap_file(&rqst, &len);
#endif
  buffer = nv_map_read(&rqst, NULL);
#ifdef _USE_SHADOWCOPY
  buffer = rqst.log_ptr;
#endif
  //assert(buffer);
  return buffer;
}

void* nvread_id_(unsigned int varid, int id)
{
  void *buffer = NULL;
  rqst_s rqst;
  size_t len=0;

  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.no_dram_flg = 1;
  rqst.id =  varid;
  //fprintf(stdout,"proc %d var %s\n",id, rqst.var_name);
#ifdef _USE_BASIC_MMAP
  return read_mmap_file(&rqst, &len);
#endif
  buffer = nv_map_read(&rqst, NULL);

#ifdef _USE_SHADOWCOPY
  buffer = rqst.log_ptr;
#endif
  assert(buffer);

  return buffer;
}

int mallocid = 0;
void* npvalloc_( size_t size)
{

  struct rqst_struct nprqst;
  int id =  BASEID_GET();
  void *ret = NULL;

  nprqst.pid = id+1;
  nprqst.bytes = size;
  //ret = malloc( size);
  ret = np_malloc( size, &nprqst);
  //fprintf(stdout,"invoking npmalloc %lu\n", (unsigned long)ret);
  return ret;
}

void npvfree_(void* mem)
{
  np_free(mem);
}


extern "C" {

void* npv_c_alloc_( size_t size, unsigned long *ptr)
{

  struct rqst_struct nprqst;
  int id =  BASEID_GET();
  void *ret = NULL;

  nprqst.pid = id+1;
  nprqst.bytes = size;
  //  fprintf(stdout,"invoking npv_c_alloc_\n");
  //pthread_mutex_lock(&alloc_mtx);
  ret = np_malloc( size, &nprqst);
  //pthread_mutex_unlock(&alloc_mtx);
  return ret;
}

}

extern "C" {
void* npv_c_realloc_( void*ptr, size_t size){

  return np_realloc(ptr,size);

}
}

extern "C" {
void npv_c_free_(void* mem)
{
  pthread_mutex_lock(&dealloc_mtx);
  np_free(mem);
  pthread_mutex_unlock(&dealloc_mtx);
}
}
extern "C" {
/*void* malloc(size_t size) {

  void *ret = NULL;
  //return calloc(size, sizeof(void));
  //pthread_mutex_lock(&alloc_mtx);
  ret = npv_c_alloc_(size, NULL);
  //pthread_mutex_unlock(&alloc_mtx);
  return ret;

}
void free(void* mem)
{
  npv_c_free_(mem);
}
void* realloc( void*ptr, size_t size){

  fprintf(stdout,"recv realloc size %u\n", size);  
  return np_realloc(ptr,size);

}*/
}


/*static void
fault_handler(int sig, siginfo_t *si, void *unused)
{
  int curr_chunkid=0;
  size_t length = disable_alloc_prot(si->si_addr);
  assert(length > 0);
}*/

extern "C" {

void* p_c_nvalloc_id( size_t size, char *var, 
       unsigned int *objid)
{

  void *buffer = NULL;
  rqst_s rqst;
  int id = BASEID_GET();
  size_t varsz=0;

  g_mypid = id+1;
  rqst.pid = id+1;
  rqst.id =0;
  rqst.bytes = size;
  rqst.commitsz = size;
  rqst.log_ptr = NULL;

#ifdef _USE_SHADOWCOPY
  rqst.no_dram_flg = 0;
#else
  rqst.no_dram_flg = 1;
#endif
  assert(var);

  varsz = strlen(var);
  memcpy(rqst.var_name,var,varsz);
  rqst.var_name[varsz] = 0;

#ifdef _USE_BASIC_MMAP
  return create_mmap_file(&rqst);
#endif

#ifdef _ENABLE_RESTART
  buffer = nv_map_read(&rqst, NULL);
  if(buffer) {
     //fprintf(stdout,"loading restart data %s \n", rqst.var_name);
     return buffer;
  }
#endif

#ifdef _USE_SHADOWCOPY

#ifdef _USE_UNDO_LOG
  buffer = je_malloc_((size_t)size, &rqst);
  assert(buffer);
#else
  je_malloc_((size_t)size, &rqst);
  buffer = rqst.log_ptr;
  assert(rqst.log_ptr);
#endif

#else //NOT _USE_SHADOWCOPY
  buffer = (void *)je_malloc_((size_t)size, &rqst);
  assert(buffer);
#endif
  *objid=rqst.id;

  return buffer;
}

void* p_c_nvalloc_( size_t size, char *var, int rqstid)
{
  void *buffer = NULL;
  rqst_s rqst;
  int id = BASEID_GET();
  size_t varsz=0;

#ifdef _USE_CMU_NVMALLOC
  if(!cmu_nvallocinit){
    nvmalloc_init(MAX_CMU_PAGES, 0);
    cmu_nvallocinit = 1;
  }
  return nvmalloc(size);
#endif

  g_mypid = id+1;
  rqst.pid = id+1;
  rqst.id =0;
  rqst.bytes = size;
  rqst.commitsz = size;
  rqst.log_ptr = NULL;

#ifdef _USE_SHADOWCOPY
  rqst.no_dram_flg = 0;
#else
  rqst.no_dram_flg = 1;
#endif

  if(var !=NULL){
    varsz = strlen(var);
    memcpy(rqst.var_name,var,varsz);
    rqst.var_name[varsz] = 0;
  }else{
    rqst.id = rqstid;
  }

#ifdef _USE_BASIC_MMAP
  //fprintf(stdout,"creating basic map file \n");
  return create_mmap_file(&rqst);
#endif

#ifdef _ENABLE_RESTART
  buffer = nv_map_read(&rqst, NULL);
  if(buffer) {
    //fprintf(stdout,"loading restart data %s \n", rqst.var_name);
    return buffer;
  }else {

  }
#endif


#ifdef _USE_SHADOWCOPY
  //#ifdef _USE_TRANSACTION

#ifdef _USE_UNDO_LOG
  buffer = je_malloc_((size_t)size, &rqst);
  assert(buffer);
#else
  je_malloc_((size_t)size, &rqst);
  buffer = rqst.log_ptr;
  assert(rqst.log_ptr);
#endif

#else //NOT _USE_SHADOWCOPY
  buffer = (void *)je_malloc_((size_t)size, &rqst);
  //assert(buffer);
  //fprintf(stdout,"c_io.cc: creating object %s "
  //    "buffer %lu\n",var, buffer);
#endif

  return buffer;
}


void p_c_mmap_free(char *varname, void *ptr){

  rqst_s  rqst;
#ifdef _USE_BASIC_MMAP

  if(varname !=NULL){
    memcpy(rqst.var_name,varname,MAXOBJNAMELEN);
    rqst.var_name[MAXOBJNAMELEN] = 0;
  }
  rqst.nv_ptr = ptr;
  assert(ptr);
  close_mmap_file(&rqst);
#endif
}

void p_c_free_(void *ptr) {
  //je_free(ptr);
}
}

void* nvalloc_( size_t size, char *var, int id){

  return p_c_nvalloc_(size, var,id);
}

void *nvalloc_id(size_t size, char *var, unsigned int *objid){
  return p_c_nvalloc_id(size, var, objid);
}

void* nvallocref_(size_t size, char *var, int id, unsigned long *ptr){
  void *ref = p_c_nvalloc_(size, var,id);
  *ptr = (unsigned long)ref;
}


void nvfree_(void *ptr){
  p_c_free_(ptr);
}


#ifdef _USE_BASIC_MMAP
void mmap_free(char *varname, void *ptr){

  rqst_s  rqst;

  if(varname !=NULL){
    memcpy(rqst.var_name,varname,MAXOBJNAMELEN);
    rqst.var_name[MAXOBJNAMELEN] = 0;
  }

  rqst.nv_ptr = ptr;

  close_mmap_file(&rqst);
}
#endif


extern "C" {

//Same as normal read, but also returns chunk data size
void* p_c_nvread_len(char *var, int id, size_t *chunksize)
{
  void *buffer = NULL;
  rqst_s rqst;
  size_t len=0;

  if(id == 0) {
    len = strlen(var);
    memcpy(rqst.var_name,var,len);
    rqst.var_name[len] = 0;
    rqst.id = 0;
  }else {
    rqst.id = id;
  }
    
  rqst.pid = BASEID_GET()+1;

#ifdef _USE_BASIC_MMAP
    len = strlen(var);
  assert(len);
    memcpy(rqst.var_name,var,len);
    rqst.var_name[len] = 0;
  //if(chunksize){
  //  *chunksize = rqst.commitsz;
  //}
  //fprintf(stderr, "chunksize %u\n",chunksize);
  return read_mmap_file(&rqst, chunksize);
#endif


#ifdef  _USE_SHADOWCOPY
  rqst.no_dram_flg = 0;
#else
  rqst.no_dram_flg = 1;
#endif
  buffer = nv_map_read(&rqst, NULL);
#ifdef  _USE_SHADOWCOPY
  buffer = rqst.log_ptr;
#endif

  if(chunksize){
    *chunksize = rqst.commitsz;
  }
  return buffer;
}

void* p_c_nvread_(char *var, int id)
{
  return p_c_nvread_len(var, id, NULL);
}

}

extern "C" {

void  p_c_nvcommitsz(char *ptr, size_t commitsz){

  rqst_s rqst;
  int id=0;
  size_t len =0;

  len = strlen(ptr);
  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.id = 0;
  memcpy(rqst.var_name,ptr,len);
  rqst.var_name[len] = 0;
  nv_commit_len(&rqst, commitsz);
}

void  p_c_nvcommitsz_id(unsigned int objid, size_t commitsz){

  rqst_s rqst;
  int id=0;
  size_t len =0;

  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.id = objid;
  rqst.var_name[len] = 0;
  nv_commit_len(&rqst, commitsz);
}





}

void* nvread_len(char *var, int id, size_t *chunksize) {
  p_c_nvread_len(var,id, chunksize);
}


void nvcommitsz(char *ptr, size_t commitsz){

  p_c_nvcommitsz(ptr,commitsz);
  return;
}

void nvcommitsz_id(unsigned int objid, size_t commitsz){

  p_c_nvcommitsz_id(objid,commitsz);
  return;
}



void nvdelete(char *ptr, int id){

  rqst_s rqst;
  size_t len =0;

  len = strlen(ptr);
  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.id = 0;
  memcpy(rqst.var_name,ptr,len);
  rqst.var_name[len] = 0;
  nv_delete(&rqst);
  return;
}


void nv_renameobj(char *src, char *dest){

  void *buffer = NULL;
  rqst_s rqst;
  int id=0;
  size_t len =0;

  len = strlen(src);
  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.id = 0;
  memcpy(rqst.var_name,src,len);
  rqst.var_name[len] = 0;
  nv_rename(&rqst,dest);
  return;
}

/*Finds the nvchunk corresponding to 
the pointer and flushes the cache*/
int nvsync(void *ptr, size_t len){
  //return nv_sync_obj(ptr);
#ifdef USE_CACHEFLUSH
  return nv_sync_chunk(ptr,len);
#endif
}


#ifdef _OBJNAMEMAP
/*Get the object name list
 * Problem: Works only for current
 * project only
 */
char** get_object_name_list(int pid, int *entries){

  return get_object_list(entries);
}
#endif


#ifdef _USE_TRANSACTION
extern "C" {

int commit_noarg(void) {
  nvcommit_noarg();  
}


int p_c_nvcommit(size_t size, char *var, int id)
{
  rqst_s rqst;

  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.bytes = size;
  rqst.id = 0;    
  memcpy(rqst.var_name,var,MAXOBJNAMELEN);
  nv_commit(&rqst);
}

int p_c_nvcommitobj(void *addr, int id)
{
#ifndef _DUMMY_TRANS
  return nv_commit_obj(addr);
#else
  return 0;
#endif
}

int p_c_nvcommitword(void *wordaddr)
{
#ifndef _DUMMY_TRANS
  return nv_commit_word(wordaddr);
#else
  return 0;
#endif
}
}

int nvcommitobj(void *addr, int id)
{
#ifndef _DUMMY_TRANS
  return nv_commit_obj(addr);
#else
  return 0;
#endif
}

int nvcommitword_(void *wordaddr)
{
#ifndef _DUMMY_TRANS
  return nv_commit_word(wordaddr);
#else
  return 0;
#endif
}


int nvcommit_(size_t size, char *var, int id)
{
  rqst_s rqst;
  id = BASEID_GET();
  rqst.pid = id+1;
  rqst.bytes = size;
  rqst.id = 0;
  memcpy(rqst.var_name,var,MAXOBJNAMELEN);
#ifdef NV_DEBUG
  fprintf(stdout,"commiting rqst.var_name %s\n",rqst.var_name);
#endif
  nv_commit(&rqst);
}


///////////////////////TRANSACTION BEGIN///////////////////////
extern "C" {

/*C - uses object addr to start transaction*/
int c_begin_trans_obj(void *addr, int pid){
#ifndef _DUMMY_TRANS
  return nv_begintrans_obj(addr);
#else
  return 0;
#endif
}

/*C - uses word addr to start transaction*/
int c_begin_trans_wrd(void *addr, size_t size, int pid){
#ifndef _DUMMY_TRANS
  return nv_begintrans_wrd(addr, size);
#else
  return 0;
#endif
}

int store_word(nvword_t *addr, nvword_t value) {
#ifndef _DUMMY_TRANS
  return store_log(addr,value);
#else
  return 0;
#endif
}
nvword_t load_word(nvword_t *addr){
#ifndef _DUMMY_TRANS
  return load_log(addr);
#else
  return NULL;
#endif

}

}

int begin_trans_wrd(void *addr, size_t size,int pid){
#ifndef _DUMMY_TRANS
  return nv_begintrans_wrd(addr, size);
#else
  return 0;
#endif
}

/*C++ uses object addr to start transaction*/
int begin_trans_obj(void *addr, int pid){
#ifndef _DUMMY_TRANS
  return nv_begintrans_obj(addr);
#else
  return 0;
#endif
}

#endif

///////////////////////RESTART RELATED///////////////////////
extern "C" {

void* c_load_ptr(void **ptr){

#ifdef _ENABLE_SWIZZLING
  return load_valid_addr(ptr);
#else
  return NULL;
#endif
}
}

void* load_ptr(void **ptr) {

#ifdef _ENABLE_SWIZZLING
  return load_valid_addr(ptr);
#else
  return NULL;
#endif

}

///////////////////////ALLOC RELATED///////////////////////

void* alloc_( unsigned int size, char *var, int id, int commit_size)
{
  void *buffer = NULL;
  rqst_s rqst;

  //init_checkpoint(id+1);
  //#ifdef _ASYNC_LCL_CHK
  //exclude_aff(ASYNC_CORE);
  //#endif
#ifdef ENABLE_RESTART
  buffer = nvread(var, id);
  if(buffer) {
    //fprintf(stdout, "nvread succedded \n");
    //return malloc(size);
    return buffer;
  }
#endif
  g_mypid = id+1;
  rqst.id = ++mallocid;
  rqst.pid = id+1;
  rqst.commitsz = commit_size;
#ifdef _USE_SHADOWCOPY
  rqst.no_dram_flg = 0;
#else
  rqst.no_dram_flg = 1;
#endif
  memcpy(rqst.var_name,var,MAXOBJNAMELEN);
  rqst.var_name[MAXOBJNAMELEN] = 0;
  je_malloc_((size_t)size, &rqst);
#ifdef _USE_SHADOWCOPY
  buffer = rqst.log_ptr;
#else
  buffer = rqst.nv_ptr;
#endif
  assert(buffer);
  return buffer;
}

// allocates n bytes using the 
void* my_alloc_(unsigned int* n, char *s, int *iid, int *cmtsize) {
  return alloc_(*n, s, *iid, *cmtsize);
}



int fd = -1;
void write_io_(char* fname, float *buff, int *size, int *restart) {

  if(fd == -1 || *restart)   
    fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC,0777);

  //return alloc_(*n, s, *iid);
  int sz =0;
  sz = write(fd,buff, *size*4);
  lseek(fd, *size, SEEK_SET);
  //fprintf(stdout ,"SIZE %d written %u name %s , fd %d\n",*size, sz, fname, fd);
}


void my_free_(char* arr) {
  free(arr);
}


#ifdef _USE_CHECKPOINT

int nvchkpt_all_(int *mype) {

#ifdef _NOCHECKPOINT
  return 0;
#endif
  rqst_s rqst;
  int ret =0;

#ifdef _ASYNC_LCL_CHK
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = handler;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    handle_error("sigaction");
#endif

  //Nothing to checkpoint
  //if(!init_chkpt) 
  //  return -1;
  /*gettimeofday(&g_chkpt_inter_end, NULL);
  if(*mype == 1) {
    fprintf(stdout,"CHECKPOINT TIME: %ld \n ",
        simulation_time(g_chkpt_inter_strt, g_chkpt_inter_end));
  }*/
  *mype = BASEID_GET();
  rqst.pid = *mype + 1;
  g_mypid =   rqst.pid;

#ifdef NV_DEBUG
  fprintf(stdout,"invoking commit\n");
#endif

  ret= nv_chkpt_all(&rqst, 0);

  //get_aff();
  gettimeofday(&g_chkpt_inter_strt, NULL);
  iter_count++;

#ifdef _ASYNC_LCL_CHK
  if(!thread_init) {
    start_async_commit();
    thread_init = 1;
  }
#endif



#ifdef _CHKPTIF_DEBUG
  fprintf(stdout,"proc %d exiting chkpt\n",*mype);
#endif
  return ret;
}






void* nv_shadow_copy(void *src_ptr, size_t size, char *var, int pid, size_t commit_size)
{
  totalsize += size;
  void *buffer = NULL;
  rqst_s rqst;
  int id =0;

  id = BASEID_GET();
  //init_checkpoint(id+1);
  g_mypid = id+ 1;

  //fprintf(stdout,"calling nv_shadow_copy \n");
#ifdef _ASYNC_LCL_CHK
  if(!set_affinity) {
    exclude_aff(ASYNC_CORE);
    set_affinity =1;
  }
#endif
  //#ifdef ENABLE_RESTART
  buffer =  nvread_(var, id+1);
  if(buffer) {

    if(strstr(var,"atom:v")) {
      double *src = (double *)buffer;
      fprintf(stdout,"nvread delme $%s$ %lf %lu \n",var, src[0], (unsigned long)buffer);
    }
    memcpy(src_ptr,buffer,size);
    //exit(0);
    return buffer;
  }
  //#endif
  rqst.pid = id+1;
  rqst.commitsz = size;
  rqst.var_name = (char *)malloc(MAXOBJNAMELEN);
#ifdef _USE_SHADOWBUFF
  rqst.no_dram_flg = 0;
  rqst.log_ptr = src_ptr;
#else
  rqst.no_dram_flg = 1;
#endif

  fprintf(stdout,"SRC PTR %lu \n",(unsigned long)src_ptr);
  rqst.logptr_sz = size;
  memcpy(rqst.var_name,var,MAXOBJNAMELEN);
  rqst.var_name[MAXOBJNAMELEN] = 0;
  je_malloc_((size_t)size, &rqst);
#ifdef _USE_SHADOWBUFF
  buffer = rqst.log_ptr;
#else
  buffer = rqst.nv_ptr;
#endif
  assert(buffer);
  init_chkpt= 1;

#ifdef _ASYNC_LCL_CHK
  /*    gettimeofday(&g_chkpt_inter_strt, NULL);
  if(!thread_init) {
    precommit = 1;
    start_async_commit();
    thread_init = 1;
  }*/
#endif


  /*    if(!thread_init) {
#ifdef _ASYNC_LCL_CHK
            start_async_commit();
#endif
      thread_init = 1;
       }*/
  return buffer;
}


int* create_shadow(int*& x, int y, char const* s, int n) {

  nv_shadow_copy((void *)x, y*sizeof(int), (char *)s, n,  y*sizeof(int));
  return x;
}



int** create_shadow(int**&x, int y, int z, char const* s, int n) {

  nv_shadow_copy((void *)x[0], y*z*sizeof(int), (char *)s, n,  y*z*sizeof(int));

  return x;
}


double* create_shadow(double*& x, int y, char const* s, int n) { 

  nv_shadow_copy((void *)x, y*sizeof(double), (char *)s, n,  y*sizeof(double));
  return x;

}

double** create_shadow(double**& x, int y, int z, char const* s, int n) { 

  void *ptr;

  ptr = (void *)x[0];
  nv_shadow_copy(ptr, y*z*sizeof(double), (char *)s, n,  y*z*sizeof(double));

  if(strstr(s,"atom:v")) {
    delme = (double *)ptr;
  }

  return x;
}

#ifdef _USE_GPU_CHECKPT

int nvchkpt_veclist(void *vec) {
  return nvchkptvec(vec);
}
#endif


#endif



unsigned int uid=0;
void *prev_ptr;
size_t prev_size=0;

#ifdef _USE_HOTPAGE
void* malloc(size_t size){

  void *ptr;  
  void* (*libc_malloc)(size_t);
  struct sigaction sa;
  struct sched_param param;

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = fault_handler;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    handle_error("sigaction");

  //pthread_mutex_lock(&alloc_mtx);
#ifdef _USE_CMU_NVMALLOC
  if(!cmu_nvallocinit){
    nvmalloc_init(MAX_CMU_PAGES, 0);
    cmu_nvallocinit = 1;
  }
  ptr = nvmalloc(size);
#else
  //ptr = npv_c_alloc_(size, NULL);
  fprintf(stderr,"before malloc\n");
  *(void **)(&libc_malloc) = dlsym(RTLD_NEXT, "malloc");
  fprintf(stderr,"malloco %lu \n", prev_ptr);
  if(prev_ptr) {
    add_map(prev_ptr,prev_size);
    set_chunk_protection(prev_ptr, prev_size, 1);
  }
  prev_ptr = libc_malloc(size);
#endif

  fprintf(stdout,"new: adding map \n");
  add_map(mem,sz);

  //pthread_mutex_unlock(&alloc_mtx);
  return prev_ptr;
}

void* operator new(size_t sz)
{
  void* mem;
  //pthread_mutex_lock(&alloc_mtx);
  mem = malloc(sz);
  //pthread_mutex_unlock(&alloc_mtx);
  return mem;
}

void operator delete(void* ptr)
{
  //pthread_mutex_lock(&dealloc_mtx);
  free(ptr);
  //pthread_mutex_unlock(&dealloc_mtx);
}
#endif


#ifdef _USE_MALLOCLIB
void* malloc(size_t size){

  void *ptr;  

  pthread_mutex_lock(&alloc_mtx);

#ifdef _USE_CMU_NVMALLOC
  if(!cmu_nvallocinit){
    nvmalloc_init(MAX_CMU_PAGES, 0);
    cmu_nvallocinit = 1;
  }
  ptr = nvmalloc(size);
#else
  ptr = npv_c_alloc_(size, NULL);
#endif
  pthread_mutex_unlock(&alloc_mtx);
  return ptr;
}

void* calloc(size_t typesz, size_t data){

  void *ptr;
  pthread_mutex_lock(&alloc_mtx);
#ifdef _USE_CMU_NVMALLOC
  if(!cmu_nvallocinit){
    nvmalloc_init(MAX_CMU_PAGES, 0);
    cmu_nvallocinit = 1;
  }

  ptr = nvmalloc(data*typesz);
#else
  ptr = npv_c_alloc_(typesz*data, NULL);
#endif
  pthread_mutex_unlock(&alloc_mtx);
  return ptr;

}

void* realloc(void *ptr, size_t size) {

  pthread_mutex_lock(&alloc_mtx);
  //fprintf(stdout,"custome realloc %u\n", size);
  ptr= np_realloc(ptr, size);
  pthread_mutex_unlock(&alloc_mtx);
  return ptr;
}


void free(void *ptr){

  pthread_mutex_lock(&dealloc_mtx);
#ifdef _USE_CMU_NVMALLOC
  //nvfree(ptr);
#else
  //np_free(ptr);
#endif
  pthread_mutex_unlock(&dealloc_mtx);
}


void* operator new(size_t sz)
{
  void* mem;

  pthread_mutex_lock(&alloc_mtx);

#ifdef _USE_CMU_NVMALLOC
  if(!cmu_nvallocinit){
    nvmalloc_init(MAX_CMU_PAGES, 0);
    cmu_nvallocinit = 1;
  }
  mem = nvmalloc(sz);
#else
  mem = npv_c_alloc_(sz, NULL);
#endif

  pthread_mutex_unlock(&alloc_mtx);
  return mem;
}


void operator delete(void* ptr)
{
  //fprintf(stdout,"deleting\n");
  //free(ptr);
  pthread_mutex_lock(&dealloc_mtx);
#ifdef _USE_CMU_NVMALLOC
  //nvfree(ptr);
#else
  //np_free(ptr);
#endif
  pthread_mutex_unlock(&dealloc_mtx);
}

#endif










