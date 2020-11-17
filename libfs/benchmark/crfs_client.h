#ifndef DEVFS_CLIENT_H_
#define DEVFS_CLIENT_H_

#define PAGE_SIZE 4096
#define DATA_SIZE 4096
#define QSIZE 409600
#define BLOCKSIZE 512
#if defined(_POSIXIO)
	#define TEST "/mnt/ramfs/test"
#else
	#define TEST "/mnt/ram/test"
#endif
#if defined(_KERNEL_TESTING)
	#define OPSCNT 1
#else
	//#define OPSCNT 1024
	//#define OPSCNT 3145728
	#define OPSCNT 524288
	//#define OPSCNT 1048576
#endif
#define CREATDIR O_RDWR | O_CREAT //| O_TRUNC
#define READIR O_RDWR
//#define CREATDIR O_RDWR|O_CREAT|O_TRUNC //|O_DIRECT
#define MODE S_IRWXU //S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

#define WRITER_CNT 1
#define READER_CNT 4
 
#define MAX_SLBA 16*1024*1024*1024L
#define GB 1024*1024L*1024L;

#endif /*DEVFS_CLIENT_H_ */

