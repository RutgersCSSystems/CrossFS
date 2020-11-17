
/*
 * Snappy NVM test
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <nv_def.h>
#include <nv_map.h>
#include <c_io.h>
#include <snappy.h>

#ifdef ENABLE_MPI_RANKS
#include "mpi.h"
#endif

#define USECSPERSEC 1000000
#define pthread_attr_default NULL
#define MAX_THREADS 2
#define BASE_PROC_ID 20000

unsigned int procid;
void run_test(char a);
static size_t size = 1048576; /* 1MB */
static int numobjs = 1000;

int main(int argc, char *argv[])
{
  if(argc < 2) {
    fprintf(stdout, "enter read (r) or write(w) mode \n");
    exit(0);
  }
     nvinit_(BASE_PROC_ID);

#ifdef ENABLE_MPI_RANKS
  MPI_Init (&argc, &argv);
#endif

  //printf("Starting test...\n");

  if(!strcmp(argv[1], "w"))
    run_test('w');
  else
     run_test('r');

  exit(0);
}

void run_test(char r)
{
  register int i, isCompress=0;
  register size_t request_size = size;
  struct timeval start, end, null, elapsed, adjusted;
  register char *buf, *src;
  char varname[100];
  size_t destlen = 0;
  char *compressed = NULL;

  struct timespec startpoll, endpoll;
  startpoll.tv_sec = 0;
  endpoll.tv_nsec = 10000L;

#ifdef ENABLE_MPI_RANKS
  int rank;
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  fprintf(stderr,"rank %d \n",rank);
  procid = rank + 1;
#else
  procid = 1;
#endif
  /* Time a null loop.  We'll subtract this from the final
   * malloc loop results to get a more accurate value.*/
  null.tv_sec = end.tv_sec - start.tv_sec;
  null.tv_usec = end.tv_usec - start.tv_usec;
  if (null.tv_usec < 0) {
    null.tv_sec--;
    null.tv_usec += USECSPERSEC;
  }
  src = (char *)malloc(size);
  assert(src);
  for(unsigned int j=0; j < size; j++)
	  src[j]= 'a' + (char)i;

  /*Run the real malloc test*/
  gettimeofday(&start, NULL);

  for (i = 0; i < numobjs; i++) {

    bzero(varname,0);
    sprintf(varname,"%d",i);
    strcat(varname,"_");
    strcat(varname,(char* )"buf");
    if(r == 'w') {
      buf = (char *)nvalloc_(size+1,varname,BASE_PROC_ID);
      assert(buf);
      memcpy(buf, src, size);
      buf[size] = 0;
      //printf("Allocated %d objects \n",i);

    }else{

      isCompress = 1;
tryagain:
      buf = (char *)nvread_(varname,BASE_PROC_ID);
      if(buf == NULL) {
    	  nanosleep(&startpoll , &endpoll);
    	  goto tryagain;
      }
      request_size = strlen(buf);
      /*Allocate to the size of source*/
      compressed = (char *)malloc(request_size);
	  snappy::RawCompress(buf, strlen(buf),compressed,&destlen);
     }
  }
  gettimeofday(&end, NULL);
  elapsed.tv_sec = end.tv_sec - start.tv_sec;
  elapsed.tv_usec = end.tv_usec - start.tv_usec;
  if (elapsed.tv_usec < 0) {
    elapsed.tv_sec--;
    elapsed.tv_usec += USECSPERSEC;
  }
  /*Adjust elapsed time by null loop time*/
  adjusted.tv_sec = elapsed.tv_sec - null.tv_sec;
  adjusted.tv_usec = elapsed.tv_usec - null.tv_usec;
  if (adjusted.tv_usec < 0) {
    adjusted.tv_sec--;
    adjusted.tv_usec += USECSPERSEC;
  }
  if(isCompress) {
    printf("Compressed %d objects bytes %zu adjusted timing: %lu.%06lu "
       "seconds for %d \n", i, size*i,elapsed.tv_sec, elapsed.tv_usec);
  }else {
	printf("Generated %d objects bytes %zu adjusted timing: %lu.%06lu "
	   "seconds for %d \n", i, size*i,elapsed.tv_sec, elapsed.tv_usec);
  }
  pthread_exit(NULL);
}
