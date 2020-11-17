#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef ENABLE_MPI_RANKS
	#include <mpi.h>
#endif

#include "include/nv_map.h"

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

int main(int argc, char *argv[])
{
    int count =0;  
    int proc_id = atoi(argv[1]);
    int rank, numprocs, dest_node, src_node;
    size_t bytes =0;

#ifdef ENABLE_MPI_RANKS
       MPI_Status status;
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Bcast(&numprocs, 1, MPI_INT, 0, MPI_COMM_WORLD);

	proc_rmt_chkpt(proc_id, &bytes,1, numprocs, rank);
	MPI_Barrier(MPI_COMM_WORLD);
exit:
	MPI_Finalize();
#endif 
    return 0;
}
