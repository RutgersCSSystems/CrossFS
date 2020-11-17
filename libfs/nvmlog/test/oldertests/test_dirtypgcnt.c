#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <mpi.h>

#define FILEPATH "/tmp/mmapped3.bin"
#define NUMINTS  (90)
#define FILESIZE (NUMINTS * sizeof(int))
#define __NR_nv_mmap_pgoff     301 
#define __NR_copydirtpages 304
#define MAP_SIZE 1024 * 10
#define SEEK_BYTES 1024 * 10
#define PAGE_SIZE 4096
#define INVALID_INPUT -2;
#define INTERGER_BUFF 1000

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
	int noPersist;
	int refcount;
};


int main(int argc, char *argv[])
{
    int i;
    int fd;
    int result;
    char *map;
    void *start_addr;
	int count =0;  
    unsigned long offset = 0;
    int proc_id = atoi(argv[1]);
	int chunk_id =atoi(argv[2]);;
	struct nvmap_arg_struct a;
	int node, numprocs, dest_node, src_node;
    MPI_Status status;
   
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Bcast(&numprocs, 1, MPI_INT, 0, MPI_COMM_WORLD);

    i =0;
	a.fd = -1;
	a.offset = offset;
	a.chunk_id =chunk_id;
	a.proc_id =proc_id;
	a.pflags = 1;
	a.noPersist = 0;

	printf("going to mmap readd \n");
	size_t bytes = INTERGER_BUFF * sizeof(unsigned int);
	void *dest =   malloc(bytes);
	unsigned int numpages =syscall(__NR_copydirtpages, &a, dest);
	fprintf(stderr, "numpages %ld, bytes %u \n", numpages, numpages*PAGE_SIZE);

	memset(dest, 1, INTERGER_BUFF * sizeof(unsigned int));

	MPI_Barrier(MPI_COMM_WORLD);
exit:
	MPI_Finalize();
    return 0;
}
