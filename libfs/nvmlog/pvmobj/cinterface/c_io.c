/******************************************************************************
* FILE: mergesort.c
* DESCRIPTION:  
*   The master task distributes an array to the workers in chunks, zero pads for equal load balancing
*   The workers sort and return to the master, which does a final merge
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include "util_func.h"
#include <sys/time.h>
#include <math.h>
#include <sched.h>
#include "jemalloc/jemalloc.h"
#include "nv_map.h"
#include <pthread.h>
#include "nv_rmtckpt.h"
#include <signal.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include "error_codes.h"
#include "nv_def.h"
#include "np_malloc.h"


//#define _CHKPTIF_DEBUG
//#define _NONVMEM
#define _NVMEM
//#define IO_FORWARDING
//#define LOCAL_PROCESSING
#define FREQUENCY 1
//#define FREQ_MEASURE
#define MAXVARLEN 30

#define BASEID 300

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
#ifdef FREQ_MEASURE
//For measurement
double g_iofreq_time=0;
#endif

pthread_mutex_t precommit_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t precommit_cond = PTHREAD_COND_INITIALIZER;
int precommit=0;
int curr_chunkid =0;
int prev_chunkid =0;


#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)


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


#if 0
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
   int return_val = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

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
}

#endif


int mysleep(int time) {
	struct timeval tv;
	tv.tv_sec =time;
	tv.tv_usec = 0;
	select(0, NULL, NULL, NULL, &tv);
}	



void* set_protection(void *arg)
{


	long  chkpt_itr_time = 0;
	long itr;
	long simtime;
	long sleep_time;
	int target_chunk =0;

#if 0
	assign_aff();
#endif

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

}

#ifdef _ASYNC_LCL_CHK
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
	/*pthread_attr_init(&lp_attr);
	pthread_attr_setinheritsched(&lp_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&lp_attr, SCHED_FIFO);
	min_priority = 15; //sched_get_priority_min(SCHED_FIFO);
	param.sched_priority = min_priority;
	pthread_attr_setschedparam(&lp_attr, &param);*/

	//iret1 = pthread_create(&thread1,&lp_attr, set_protection, (void*)NULL);

    /* Create independent threads each of which will execute function */
    iret1 = pthread_create(&thread1,NULL, set_protection, (void*)NULL);
}

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


void *nv_jemalloc(size_t size, rqst_s *rqst) {

	return je_malloc_((size_t)size, rqst);
}

void* nvread_(char *var, int id)
{
    void *buffer = NULL;
    rqst_s rqst;


    id = BASEID;
    rqst.pid = id+1;
    rqst.var_name = (char *)malloc(MAXVARLEN);
    memcpy(rqst.var_name,var,MAXVARLEN);
    rqst.var_name[MAXVARLEN] = 0;
	rqst.no_dram_flg = 1;
    fprintf(stdout,"proc %d var %s\n",id, rqst.var_name);
    buffer = nv_map_read(&rqst, NULL);
    //buffer = rqst.log_ptr;

    if(rqst.var_name)
        free(rqst.var_name);

    return buffer;
}


/*--------------------------------------*/
//testing code. delete after analysis
//
#ifdef _FAULT_STATS
/*int num_faults =0;
void *g_tmpbuff;
size_t g_tmpbuff_sz;
size_t total_pages;

static void
temp_handler(int sig, siginfo_t *si, void *unused)
{

   size_t len = 4096;
   void *addr = (unsigned long)si->si_addr & ((0UL - 1UL) ^ (4096 - 1));
   int diff =   (unsigned long)si->si_addr - (unsigned long)addr;

   num_faults++;

    if (mprotect(addr, 4096, PROT_READ|PROT_WRITE)==-1) {
        fprintf(stdout,"%lu\n", si->si_addr);
        perror("mprotect");
        exit(-1);
    }

}


int register_handler() {

    struct sigaction sa;
    struct sched_param param;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = temp_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
        handle_error("sigaction");

}

int temp_protection(void *addr, size_t len, int flag){


    if (mprotect(addr,len, PROT_READ)==-1) {
        fprintf(stdout,"%lu \n", (unsigned long)addr);
        perror("mprotect: temp_prot");
      	exit(-1); 
    }
    
        return 0;
}*/

#endif //#ifdef _FAULT_STATS
/*-------------------------------------------------------*/



int mallocid = 0;

void* npvalloc_( size_t size)
{

	struct rqst_struct nprqst;
	int id =  BASEID;
	void *ret = NULL;

	nprqst.pid = id+1;
	nprqst.bytes = size;
	//fprintf(stdout,"invoking npmalloc \n");
	ret = np_malloc( size, &nprqst);
	return ret;
}

void npvfree_(void* mem)
{
	np_free(mem);
}




void* nvalloc_( size_t size, char *var, int id)
{
	void *buffer = NULL;
	rqst_s rqst;

	id = BASEID;

#ifdef ENABLE_RESTART
	/*buffer = nvread(var, id);
	if(buffer) {
		//fprintf(stdout, "nvread succedded \n");
		//return malloc(size);
		return buffer;
	 }*/
#endif

	g_mypid = id+1;
	rqst.id = ++mallocid;
	rqst.pid = id+1;
	rqst.commitsz = size;
	rqst.no_dram_flg = 1;
	rqst.var_name = (char *)malloc(MAXVARLEN);
	memcpy(rqst.var_name,var,MAXVARLEN);
	rqst.var_name[MAXVARLEN] = 0;
	je_malloc_((size_t)size, &rqst);
	//buffer = rqst.log_ptr;
	buffer = rqst.nv_ptr;
	assert(buffer);

	/*fprintf(stdout, "allocated nvchunk %s,"
		"size %u addr: %lu\n,",
		 rqst.var_name,
		 size,
		 (unsigned long)buffer);*/

	if(rqst.var_name)
	  free(rqst.var_name);
	return buffer;
}



void* alloc_( unsigned int size, char *var, int id, int commit_size)
{
	void *buffer = NULL;
	rqst_s rqst;

	init_checkpoint(id+1);

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
	rqst.no_dram_flg = 0;
	rqst.var_name = (char *)malloc(MAXVARLEN);
	memcpy(rqst.var_name,var,MAXVARLEN);
	rqst.var_name[MAXVARLEN] = 0;
	je_malloc_((size_t)size, &rqst);
	buffer = rqst.log_ptr;
	assert(buffer);
	if(rqst.var_name)
	  free(rqst.var_name);

	//if(g_mypid == 1)
    //fprintf(stdout, "allocated total %d bytes %s \n", size,var);	
	//fprintf(stdout,"proc %d leaving alloc_\n",id);

#ifdef _FAULT_STATS
	if(g_mypid == 1) {

		register_handler();
		total_pages = total_pages + (size/4096);	
		//if(strstr(var,"zion")) {
		//	g_tmpbuff_sz = size;
		//	g_tmpbuff = buffer;	
	  temp_protection(buffer,commit_size, PROT_READ);
	}
#endif

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
  

int nvchkpt_all_(int *mype) {

#ifdef _NOCHECKPOINT
	return 0;
#endif

	//return 0;
	rqst_s rqst;
	int ret =0;
   	struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
   	sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
   	    handle_error("sigaction");

	/*gettimeofday(&g_chkpt_inter_end, NULL);
	if(*mype == 1) {
		fprintf(stdout,"CHECKPOINT TIME: %ld \n ",
        simulation_time(g_chkpt_inter_strt, g_chkpt_inter_end));
	}*/

	rqst.pid = *mype + 1;
	g_mypid =   rqst.pid;


	if(iter_count % 10000 == 0)
		ret= nv_chkpt_all(&rqst, 1);
	else
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


#ifdef _FAULT_STATS
	if(g_mypid == 1)
		fprintf(stdout,"num_faults %d total_pages %d \n",num_faults, total_pages);
	num_faults = 0;
#endif

#ifdef _CHKPTIF_DEBUG
	fprintf(stdout,"proc %d exiting chkpt\n",*mype);
#endif
	return ret;
}


void *nv_restart_(char *var, int *id) {
	
	//return nvread(var, *id);
}


int app_stop_(int *mype){

 return	app_exec_finish(*mype);
}



void* nv_shadow_copy(void *src_ptr, size_t size, char *var, int pid, size_t commit_size)
{
        void *buffer = NULL;
        rqst_s rqst;
		int id =0;

		id = pid;
        init_checkpoint(id+1);
		g_mypid = id+ 1;


//#ifdef _ASYNC_LCL_CHK
		if(!set_affinity) {
	         //exclude_aff(ASYNC_CORE);
			 set_affinity =1;
		}
//#endif


#ifdef ENABLE_RESTART
        buffer = nvread(var, id);
        if(buffer) {
           //fprintf(stdout, "nvread succedded \n");
           //return malloc(size);
           return buffer;
        }
#endif
        rqst.pid = id+1;
        rqst.commitsz = size;
        rqst.var_name = (char *)malloc(MAXVARLEN);
        rqst.no_dram_flg = 1;
        rqst.log_ptr = src_ptr;
		rqst.logptr_sz = size;
        memcpy(rqst.var_name,var,MAXVARLEN);
        rqst.var_name[MAXVARLEN] = 0;
        je_malloc_((size_t)size, &rqst);
        buffer = rqst.nv_ptr;
        assert(buffer);
        if(rqst.var_name)
          free(rqst.var_name);

#ifdef _ASYNC_LCL_CHK
/*    gettimeofday(&g_chkpt_inter_strt, NULL);
	if(!thread_init) {
		precommit = 1;
		start_async_commit();
		thread_init = 1;
	}*/
#endif


/*		if(!thread_init) {
#ifdef _ASYNC_LCL_CHK
	        	start_async_commit();
#endif
			thread_init = 1;
   		}*/
        return buffer;
}


/* int* create_shadow(int*& x, int y, char const* s, int n) { 

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

	nv_shadow_copy((void *)x[0], y*z*sizeof(double), (char *)s, n,  y*z*sizeof(double));
	return x;
 }
*/

