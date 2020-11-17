#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define FILEPATH "/tmp/mmapped3.bin"
#define NUMINTS  (90)
#define FILESIZE (NUMINTS * sizeof(int))

#define __NR_nv_mmap_pgoff     302 

#define MAP_SIZE  8096 
#define SEEK_BYTES 1024 * 1024 * 5

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
};

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
        /*if(idxa%10 ==0)
        addr[cntr] = '\n';
        else*/
        addr[cntr] = ' ';
        cntr++;
    }
    return cntr;

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
	int chunk_id = 143;
    int proc_id = 23;
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

   printf("going to mmap \n");

    i =0;

	a.fd = fd;
	a.offset = offset;
	a.chunk_id =chunk_id;
	a.proc_id = proc_id;

   for(count =0; count < 1; count++) {

		map = (char *) syscall(__NR_nv_mmap_pgoff, 0, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE, &a);
	    if (map == MAP_FAILED) {
			close(fd);
			perror("Error mmapping the file");
			exit(EXIT_FAILURE);
	    }

	    printf("map %lu\n", (unsigned long)map);

    	/*for (i = 0; i < MAP_SIZE; ++i) {
		        map[i] = 'a';
		        printf("%c \n",*(map+i));
		  }
      }*/


	if( generate_random_text( map, MAP_SIZE, MAP_SIZE/10)   == -1){
            printf("random text generation failed \n");
            exit(0);
        }

		//fprintf(stdout, "%s \n", map);
	}


	map_read = (char *) syscall(__NR_nv_mmap_pgoff, 0, MAP_SIZE,  PROT_READ, MAP_PRIVATE, &a );
    if (map_read == MAP_FAILED) {
	    close(fd);
    	perror("Error mmapping the file");
	    exit(EXIT_FAILURE);
    }

    // Read the file int-by-int from the mmap
    for (i = 0; i <=MAP_SIZE; i++) {
		printf("%c",*(map_read+i));
    }

	 printf("\n");

	 while(1) {


	 }


    /*if (munmap(map, FILESIZE) == -1) {
        perror("Error un-mmapping the file");
        /* Decide here whether to close(fd) and exit() or not. Depends... */
   //}

    /*map2 = mmap(0, FILESIZE, PROT_READ, MAP_SHARED, fd, 0);
    if(map2 == MAP_FAILED) {
	close(fd);
	perror("Error mmapping the file");
	exit(EXIT_FAILURE);
     }*/
     

   //map2 = map+i;
   
    /*map2 = realloc_map ( 0, FILESIZE, FILESIZE);
    if(map2 == MAP_FAILED) {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
     }

    map2 = map + i;


   for (i = 1; i <=NUMINTS; ++i) {
        map2[i] = 3 * i;
        printf("%lu \n",(map2+i));
    }*/

    /*for (i = 1; i <=NUMINTS; ++i) {
       printf( "%d \n",  map[i]); 
    }*/

   /*for (i = 1; i <=NUMINTS; ++i) {
        printf ( "%d \n", map2[i]); 
    }*/
    
   

    /* Don't forget to free the mmapped memory
 *      */
    /*if (munmap(map, FILESIZE + FILESIZE) == -1) {
	perror("Error un-mmapping the file");
	/* Decide here whether to close(fd) and exit() or not. Depends... */
    /*}

    /* Un-mmaping doesn't close the file, so we still need to do that.
 *      */
    close(fd);
    return 0;
}
