
/*
 * pvm persistent allocation test
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include "nv_map.h"
#include "c_io.h"

#define USECSPERSEC 1000000
#define BASE_PROC_ID 10000
unsigned int procid;
static unsigned size = 1024 * 1024;
static int total_iterations = 10;

char *getobjname(int id, char *base, char *buffer){

  int len = 0;

	memset(buffer, 0, len);
	len = strlen(base);
	memcpy(buffer, base, len);
	strcat(buffer, "_");
	snprintf(buffer+strlen(buffer), 8, "%d", id);
	len = strlen(buffer);
	buffer[len] = 0;
  return buffer;
}

void run_test(void* val)
{
  register unsigned int i;
  register unsigned request_size = size;
  struct timeval start, end, null, elapsed, adjusted;
  int rank = 0;
  char *ptr[10];
  char base[255]="Hello";
  char buffer[255];

  rqst_s rqst;
  nvinit_(BASE_PROC_ID);
  /*
   * Time a null loop.  We'll subtract this from the final
   * malloc loop results to get a more accurate value.
   */
  null.tv_sec = end.tv_sec - start.tv_sec;
  null.tv_usec = end.tv_usec - start.tv_usec;
  if (null.tv_usec < 0) {
    null.tv_sec--;
    null.tv_usec += USECSPERSEC;
  }

  /*
   * Run the real malloc test
   */
  gettimeofday(&start, NULL);
  int j =0;
  for (j = 0; j< total_iterations; j++) {

    unsigned char s;

    //rqst.id = j+1;
    rqst.id = 0;

    //generate name into buffer 
    getobjname(j, base, buffer);

    rqst.pid = rank+1+ BASE_PROC_ID;

    ptr[j] = (char *)nvalloc_(size,buffer,rqst.id);
    for (i = 0; i < size; i++) {
      ptr[j][i] = ptr[j][i] +1;
      s += ptr[j][i];  
    }
    s += 1;
    //Free the allocated memory
    nvfree_(ptr[j]);
  }

  for (j = total_iterations-1; j>= 0; j--) {
    unsigned char s;
    for (i = 0; i < size; i++) {
      ptr[j][i] = ptr[j][i] +1;
      s += ptr[j][i];  
    }
  }

  gettimeofday(&end, NULL);
  elapsed.tv_sec = end.tv_sec - start.tv_sec;
  elapsed.tv_usec = end.tv_usec - start.tv_usec;
  if (elapsed.tv_usec < 0) {
    elapsed.tv_sec--;
    elapsed.tv_usec += USECSPERSEC;
  }

  /*
   * Adjust elapsed time by null loop time
   */
  adjusted.tv_sec = elapsed.tv_sec - null.tv_sec;
  adjusted.tv_usec = elapsed.tv_usec - null.tv_usec;
  if (adjusted.tv_usec < 0) {
    adjusted.tv_sec--;
    adjusted.tv_usec += USECSPERSEC;
  }
  printf("Thread %lu adjusted timing: %lu.%06lu seconds for %d requests"
    " of %d bytes.\n", pthread_self(),
    elapsed.tv_sec, elapsed.tv_usec, total_iterations,
    request_size);
}

int main(int argc, char *argv[])
{
  run_test(NULL);
  exit(0);
}
