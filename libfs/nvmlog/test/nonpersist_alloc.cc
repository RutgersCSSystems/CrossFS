
/*
 * pvm Non persistent allocation test
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#include "nv_map.h"
#include "c_io.h"

#ifdef ENABLE_MPI_RANKS 
#include "mpi.h"
#endif

#define USECSPERSEC 1000000
#define BASE_PROC_ID 10000
static unsigned allocsize = 1024;
unsigned int procid;
static unsigned total_iterations = 100000;


void run_test(void* val)
{
	register unsigned int i,j;
	struct timeval start, end, elapsed;
	int rank = 0;
	rqst_s rqst;
#ifdef ENABLE_MPI_RANKS 
	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
#endif

	//Initialize nvm
	nvinit_(BASE_PROC_ID);
	/*
	 * Run the real malloc test
	 */
	gettimeofday(&start, NULL);
	for (j = 0; j< total_iterations; j++) {
		char *ptr = NULL;
		rqst.id = j+1;
		//rqst.var_name = NULL;
		rqst.pid = rank+1+ BASE_PROC_ID;
		rqst.nv_ptr =npvalloc_(allocsize);
		ptr = (char *)rqst.nv_ptr;
		for (i = 0; i < allocsize; i++) {
			ptr[i] = 'a';
		}
		npvfree_(ptr);
	}
	gettimeofday(&end, NULL);
	elapsed.tv_sec = end.tv_sec - start.tv_sec;
	elapsed.tv_usec = end.tv_usec - start.tv_usec;
	if (elapsed.tv_usec < 0) {
		elapsed.tv_sec--;
		elapsed.tv_usec += USECSPERSEC;
	}
	printf("Timing: %ld.%ld seconds for %u requests"
			" of %u bytes.\n",elapsed.tv_sec, elapsed.tv_usec, total_iterations,
			allocsize);
}

int main(int argc, char *argv[])
{
#ifdef ENABLE_MPI_RANKS	
	MPI_Init (&argc, &argv);	
#endif
	run_test(NULL);
#ifdef ENABLE_MPI_RANKS
	MPI_Barrier(MPI_COMM_WORLD);
#endif
	exit(0);
}
