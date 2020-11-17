#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define CACHE_LINE_SZ 64

#define REPEAT_COUNT 100
#define FILEPATH "/tmp/mmapped3.bin"
#define NUMINTS  (90)
#define FILESIZE (NUMINTS * sizeof(int))

#define __NR_nv_mmap_pgoff     301 
#define __NR_nv_commit		303

#define MAP_SIZE 1024 * 1024 * 100
#define SEEK_BYTES 1024 * 1024 * 5

#define INVALID_INPUT -2;


static unsigned int flushcnt=0;

void flush_cache(void *addr) {

   //while(1){
   __asm__ __volatile__ ("clflush %0" : : "m"(*addr));
   //fprintf(stdout,"flushcnt %u\n",flushcnt++);	
	//return;
	//exit(0);
	addr = addr + 1;
    //}
}


 void * realloc_map (void *addr, size_t len, size_t old_size)
  {
          void *p;

          p = mremap (addr, old_size, len, MREMAP_MAYMOVE);
          return p;
  }

struct nvmap_arg_struct{

	unsigned long fd;
	unsigned long offset;
	int chunk_id;
	int proc_id;
	int pflags;
};


//Method to generate random data
int generate_random_text( char *addr, unsigned long len, unsigned long  num_words  ) {

    unsigned long idxa =0, idxb =0;
    unsigned long cntr =0;
    int wordsize;
    int maxwordsize = 0 ;
    int idxc =0;

    maxwordsize = len / num_words;

    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (idxa = 0; idxa < num_words; idxa++) {

        wordsize = rand()%  maxwordsize;

        for (idxb = 0; idxb < wordsize; idxb++) {

            if( cntr >= len) break;

            idxc = rand()% (sizeof(alphanum)/sizeof(alphanum[0]) - 1);
            addr[cntr] = alphanum[idxc];
            cntr++;
        }
        addr[cntr] = ' ';
        cntr++;
    }
    return cntr;

}

int str_cmp( char *addr1, char *addr2, size_t len) {

	size_t idx = 0;

	if(len < 1 || !addr1 || !addr2) {
		printf("invalud len or addr \n");
		return INVALID_INPUT;
	}

	while(idx < len) {

		if( *addr1 != *addr2) {		
			printf("string not equal: addr1 %c, addr2 %c \n", *addr1 , *addr2);
			return -1;
		}
		printf ("addr1 %c, addr2 %c \n", *addr1 , *addr2);
		addr1++;
		addr2++;
		idx++;
	}

	return 0;
}

int main(int argc, char *argv[])
{
    int i;
    int fd;
    int result;
    char *map, *map2, *map_read;  /* mmapped array of int's */
    void *start_addr;
	int count =0;  
    unsigned long offset = 0;
	int chunk_id = 9;
    int proc_id = 99;
	struct nvmap_arg_struct a;

    fd = open(FILEPATH, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    if (fd == -1) {
	perror("Error opening file for writing");
	exit(EXIT_FAILURE);
    }

    result = lseek(fd, SEEK_BYTES, SEEK_SET);
    if (result == -1) {
	close(fd);
	perror("Error calling lseek() to 'stretch' the file");
	exit(EXIT_FAILURE);
    }
    
    result = write(fd, "", 1);
    if (result != 1) {
	close(fd);
	perror("Error writing last byte of the file");
	exit(EXIT_FAILURE);
    }


    i =0;

	a.fd = fd;
	a.offset = offset;
	a.chunk_id =chunk_id;
	a.proc_id = proc_id;
	a.pflags = 1;

	if ((atoi(argv[1])) == 0) {

	  printf("going to mmap write \n");

	    //map = (char *) syscall(__NR_nv_mmap_pgoff, 0, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE| MAP_ANONYMOUS, &a);
        map = (char *)mmap( 0, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE| MAP_ANONYMOUS, fd, 0);
	    if (map == MAP_FAILED) {
	 		close(fd);
			perror("Error mmapping the file");
			exit(EXIT_FAILURE);
	    }

	  printf("testing cache flush for size %u %d times\n",MAP_SIZE, REPEAT_COUNT);
	  for(count =0; count < REPEAT_COUNT; count++) {
		    //printf("map %lu\n", (unsigned long)map);
		if( generate_random_text( map, MAP_SIZE, MAP_SIZE/10)	== -1){
			printf("random text generation failed \n");
			exit(0);
		}
		void *addr_flush = map;
		int cnt =0, sum=0, rdcntr=0;
		char *rdaddr = NULL;
		char t;	
		

		for(cnt=0; cnt < MAP_SIZE; cnt += CACHE_LINE_SZ) {
	
			flush_cache(addr_flush); 
			rdaddr = (char *)addr_flush;

			//Then we read the flushed data
			for(rdcntr=0; rdcntr < CACHE_LINE_SZ; rdcntr++){
				t = rdaddr[rdcntr];
				/*some random stuff*/
				sum = t *100;
				rdaddr++;	 
			}
			sum=sum*100;
			addr_flush += CACHE_LINE_SZ;
		}

		if(result) {
			fprintf(stdout,"flush result %d \n",result);
			return -1;
		}
	}
     printf("Expected number of cache misses %d\n",(MAP_SIZE * REPEAT_COUNT)/64);

	}else {

	 printf("going to mmap readd \n");

	map_read = (char *) syscall(__NR_nv_mmap_pgoff, 0, MAP_SIZE,  PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS , &a );
	//map_read = (char *)mmap( 0, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (map_read == MAP_FAILED) {
	    close(fd);
    	perror("Error mmapping the file");
	    exit(EXIT_FAILURE);
    }

	fprintf(stdout, "\n\n\n\n");

	fprintf(stdout, "%s \n", map_read);


  	if(str_cmp(map, map_read, MAP_SIZE)) {
		printf("multipage test failed \n");
	}else {
		printf("multipage test succeeded \n");
	}

	}

     close(fd);

    return 0;
}
