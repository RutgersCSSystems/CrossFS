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

#define MAP_SIZE  4096 * 2 
#define SEEK_BYTES 1024 * 1024 * 5

 void * realloc_map (void *addr, size_t len, size_t old_size)
  {
          void *p;

          p = mremap (addr, old_size, len, MREMAP_MAYMOVE);
          return p;
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

    /*int byte_cnt = 0;
    int test_bytes = 1234;
    char *ptr = NULL;
    for (i = 0; i < 10; i++) {
        ptr = (char *) syscall(__NR_nv_mmap_pgoff,0, 4096,  PROT_READ | PROT_WRITE, MAP_PRIVATE , fd, 0);
        for ( byte_cnt=0; byte_cnt < test_bytes; byte_cnt++)
                ptr[i] = 'b';
                //fprintf(stderr, "ptr %c\n", ptr[i]);
    }*/


    map = mmap(0, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, (unsigned long)0);
    start_addr =map;

    i =0;

   for(count =0; count < 1; count++) {

		map = (unsigned long) syscall(__NR_nv_mmap_pgoff, start_addr, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, offset);
		// map = mmap(start_addr, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, offset);

	    //map = mmap(0, MAP_SIZE,  PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, (unsigned long)0);	
	    if (map == MAP_FAILED) {
			close(fd);
			perror("Error mmapping the file");
			exit(EXIT_FAILURE);
	    }

        //sleep(20);
	}

        offset += MAP_SIZE;

	    //printf("map %lu\n", map);

     	for (i = 4096; i <= MAP_SIZE -1; ++i) {
		        map[i] = 'a';
		        printf("%c \n",*(map+i));
		  }

		  map[0] = 'a';


	/*map_read = (unsigned long) syscall(__NR_nv_mmap_pgoff, start_addr, MAP_SIZE,  PROT_READ, MAP_PRIVATE, fd, offset);
    //map_read = mmap(0, MAP_SIZE,  PROT_READ, MAP_PRIVATE, fd, (unsigned long)0);
    if (map_read == MAP_FAILED) {
	    close(fd);
    	perror("Error mmapping the file");
	    exit(EXIT_FAILURE);
    }*/

    // Read the file int-by-int from the mmap
    //for (i = 0; i <=10; i++) {
	//	printf("mapread %c \n",*(map_read+i));
   // }

  
  	

    /*if (munmap(map, FILESIZE) == -1) {
        perror("Error un-mmapping the file");
        /* Decide here whether to close(fd) and exit() or not. Depends... */
   //}

    /*map2 = mmap(0, FILESIZE, PROT_READ, MAP_PRIVATE, fd, 0);
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
