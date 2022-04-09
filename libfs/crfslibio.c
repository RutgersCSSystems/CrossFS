#define _GNU_SOURCE


#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <linux/pci.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <linux/types.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include "unvme_nvme.h"
#include "vfio.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "crfslibio.h"
#include "time_delay.h"

#ifdef _NVMFDQ
#include "nvmlog/pvmobj/nv_def.h"
#include "nvmlog/pvmobj/nv_map.h"
#include "nvmlog/pvmobj/cache_flush.h"
#include "nvmlog/pvmobj/c_io.h"
#endif


#define PAGE_SIZE 4096
#define BLOCKSIZE 512
#define FILENAME "/mnt/ram/devfile9"
#define TEST "/mnt/ram/test"
#define OPSCNT 100000
#define CREATDIR O_RDWR | O_CREAT | O_TRUNC
#define READIR O_RDWR
//#define _DEBUG

//#define CREATDIR O_RDWR|O_CREAT|O_TRUNC //|O_DIRECT
#define MODE S_IRWXU //S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
#define NUM_QUEUE_CMDS 1

/* Global variables */
static void* vsq;
static size_t cmd_nr = 1;
static int verbose_flag;
static int isread, isdelete;
static int numfs;
char fname[256];
//DevFS device
static int g_dev;

//DevFS device global queue
//void *g_vsq;
struct fd_q_mem fd_q_mempool;

//DevFS global sba
static int g_slba;

/* Configurable parameters */
unsigned int qentrycount = 2;	// Kernel per-fd queue entry count 
unsigned int schedpolicy = 0;	// Device thread scheduler policy
unsigned int devcorecnt = 4;	// Device thread number
unsigned int fdqueuesize = 4096;// FS libray per-fd queue size
int isjourn = 0;
int isexit = 0;

/* Per process cred id */
static u8 cred_id[16] = {INVALID_CRED};

/* Statistical data on queue hit rate and conflict rate */
int fp_queue_access_cnt = 0;
int fp_queue_hit_cnt = 0;
int fp_queue_conflict_cnt = 0;

/* Global open file table */
#ifdef PARAFS_SHM
struct open_file_table* ufile_table_ptr;
#define ufile_table (*ufile_table_ptr)
#else
struct open_file_table ufile_table;
#endif

/* Inode table */
uinode *inode_table = NULL;
pthread_mutex_t uinode_table_lock = PTHREAD_MUTEX_INITIALIZER;

int getargs(int argc, char **argv);

// Statistics
int total_read = 0;
int per_thread_read[MAX_THREAD_NR] = {0};
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

//#ifdef _USE_OPT
void fault_handler(int signo, siginfo_t *info, void *extra)
{
        fprintf(stderr, "Signal %d received\n", signo);
	if(isexit != 1)
	        shutdown_crfs();
	exit(-1);
}

void setHandler(void (*handler)(int,siginfo_t *,void *))
{

	//http://man7.org/linux/man-pages/man7/signal.7.html
        struct sigaction action;
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = handler;

        /*if (sigaction(SIGFPE, &action, NULL) == -1) {
                perror("sigfpe: sigaction");
                _exit(1);
        }

        if (sigaction(SIGINT, &action, NULL) == -1) {
                perror("SIGINT: sigaction");
                _exit(1);
        }

        if (sigaction(SIGHUP, &action, NULL) == -1) {
                perror("SIGHUP: sigaction");
                _exit(1);
        }

        if (sigaction(SIGCHLD, &action, NULL) == -1) {
                perror("SIGHUP: sigaction");
                _exit(1);
        }*/

        /*if (sigaction(SIGSTOP, &action, NULL) == -1) {
                perror("SIGSTOP: sigaction");
                _exit(1);
        }*/

        /*if (sigaction(SIGPIPE, &action, NULL) == -1) {
                perror("SIGPIPE: sigaction");
                _exit(1);
        }

        if (sigaction(SIGKILL, &action, NULL) == -1) {
                perror("SIGKILL: sigaction");
                _exit(1);
        }*/

        if (sigaction(SIGTERM, &action, NULL) == -1) {
                perror("SIGTERM: sigaction");
                _exit(1);
        }

        if (sigaction(SIGSEGV, &action, NULL) == -1) {
                perror("sigsegv: sigaction");
                _exit(1);
        }
        /*if (sigaction(SIGILL, &action, NULL) == -1) {
                perror("sigill: sigaction");
                _exit(1);
        }
        if (sigaction(SIGBUS, &action, NULL) == -1) {
                perror("sigbus: sigaction");
                _exit(1);
        }
        if (sigaction(SIGABRT, &action, NULL) == -1) {
                perror("SIGABRT: sigaction");
                _exit(1);
        }*/
}
//#endif


/*
 * clean up file system state and release resource after
 * shutdown_crfs() is called for some applications like
 * RocksDB
 */
int crfs_handle_exit(int fd, nvme_command_rw_t *cmd) {
        int ret = 0;

	if (cmd && cmd->common.opc == nvme_cmd_close) {
		/*
		 * If it's a close command, just call close() directly
		 * The firmfs will release its resource
		 */
		struct vfio_crfs_closefd_cmd map;
		map.argsz = sizeof(map);
		map.fd = fd;

		ret = ioctl(g_dev, VFIO_DEVFS_CLOSEFILE_CMD, &map);
		if (ret < 0) {
			fprintf(stderr,"ioctl VFIO_DEVFS_CLOSEFILE_CMD errno %d \n", errno);
			return -1;
		}
	} else {
		ret =  cmd->nlb;
	}
	return ret;
}

#ifndef PARAFS_BYPASS_KERNEL
/*
 *  DevFS write
 */
int vfio_crfs_queue_write(int dev, int fd, void *vsq, int vsqlen) {

	long ret;

#if defined(_DEBUG)
	fprintf(stderr,"******vfio_crfs_queue_write******* %ld \n", ret);
#endif
	if (isexit == 1) {
		/*
		 * For some applications like RocksDB, even after benchmark finishes
		 * and device threads are terminated, the application background
		 * threads keeps doing some I/O operations like close() or append to
		 * the log files. If that is the case, we call crfs_handle_exit()
		 * to call close() directly to release its file system resource.
		 */
		nvme_command_rw_t *cmd = (nvme_command_rw_t*)vsq;
		return cmd->nlb;
	}

#ifdef ASSERT_ENABLE
	assert(vsq != NULL);
	assert(vsqlen > 0);
#endif

	struct vfio_crfs_rw_cmd map = {
			.argsz = sizeof(map),
			.flags = (VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE),
			.vaddr = (__u64)vsq,
			.iova = (__u64)0, //dev->iova,
			.size = (__u64)0,
			.fd = fd,
			.cmd_count = vsqlen,
	};
	ret = ioctl(dev, VFIO_DEVFS_RW_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_RW_CMD errno %d \n", errno);
		return -1;
	}
#if defined(_DEBUG)
	fprintf(stderr,"******vfio_crfs_queue_write******* %ld \n", ret);
#endif
	return ret;
}
#else
/*
 *  DevFS write
 */
int vfio_crfs_queue_write(int dev, int fd, void *vsq, int vsqlen) {

	long ret;

#ifdef ASSERT_ENABLE
	assert(vsq != NULL);
	assert(vsqlen > 0);
#endif

#ifdef PCIE_DELAY
	emulate_pcie_latency();
#endif

#ifdef FLUSH_CACHE
	flush_cache(vsq, sizeof(nvme_command_rw_t));
#endif

	nvme_command_rw_t *cmd = (nvme_command_rw_t*)vsq;

	if (isexit == 1) {
		/*
		 * For some applications like RocksDB, even after benchmark finishes
		 * and device threads are terminated, the application background
		 * threads keeps doing some I/O operations like close() or append to
		 * the log files. If that is the case, we call crfs_handle_exit()
		 * to call close() directly to release its file system resource.
		 */
		return crfs_handle_exit(fd, cmd);
	}

#if defined(_DEBUG)
	fprintf(stderr,"******vfio_crfs_queue_write begin******* %ld \n", ret);
#endif

#ifdef _USE_OPT
	while(cmd->status != (DEVFS_CMD_READY | DEVFS_CMD_FINISH | DEVFS_CMD_BUSY)) {
#else
	// Set cmd as ready
	//__sync_lock_test_and_set(&cmd->status, DEVFS_CMD_READY);
	cmd->status |= DEVFS_CMD_READY;

	// Spin until the command is handled by device;
	while(__sync_fetch_and_add(&cmd->status, 0) != (DEVFS_CMD_READY | DEVFS_CMD_FINISH | DEVFS_CMD_BUSY)) {
#endif
		/*
		 * If this command is getting called after device thread is terminated in some
		 * applications like RocksDB, just break without spin as at this time, device
		 * thread has already terminated
		 */
		if (isexit == 1) {
			cmd->ret = cmd->nlb;
			break;
		}
	}
	ret = cmd->ret;

	cmd->status = 0;

#if defined(_DEBUG)
	fprintf(stderr,"******vfio_crfs_queue_write end******* %ld \n", ret);
#endif

	return ret;
}
#endif


/**
 * NVMe submit a read write command.
 * @param   ioq         io queue
 * @param   opc         op code
 * @param   cid         command id
 * @param   nsid        namespace
 * @param   slba        startling logical block address
 * @param   nlb         number of logical blocks
 * @param   prp1        PRP1 address
 * @param   prp2        PRP2 address
 * @return  0 if ok else -1.
 */
int nvme_cmd_rw_new(int opc, u16 cid, int nsid, u64 slba, u64 nlb, u64 prp2, 
		void *vsq)
{
	nvme_command_rw_t *cmd = (nvme_command_rw_t*)vsq;
#ifndef _USE_OPT
	//memset(cmd, 0, sizeof(nvme_command_rw_t));
#endif
	cmd->common.opc = opc;
	cmd->common.cid = cid;
	cmd->common.nsid = nsid;

	cmd->common.prp2 = prp2;
	cmd->slba = slba;
	cmd->nlb = nlb;
	cmd->kalloc = 0;
	memcpy(cmd->cred_id, cred_id, CRED_ID_BYTES);

#ifdef _USE_OPT
	cmd->status = DEVFS_CMD_READY;
#endif 

#if defined(_DEBUG)
	printf("cmd->slba %llu cmd->nlb %llu\n", cmd->slba, cmd->nlb);
#endif
}


/*
 * User-level inode get and put function
 */
static uinode* get_inode(const char *fname, ufile *fp) {
	int i = 0;
	uinode *target = NULL;

	// Need mutex lock wrap around here
	pthread_mutex_lock(&uinode_table_lock);

	target = uinode_table_lookup(fname);
	if (!target) {
		target = (uinode*)malloc(sizeof(uinode));
		memset(target, 0, sizeof(uinode));
		strcpy(target->fname, fname);
		uinode_table_insert(target);
	}

	if (target) {
#ifdef PARAFS_INTERVAL_TREE
		if (target->sq_tree_init == 0) {
			target->sq_tree_init = 1;
			pthread_spin_init(&target->sq_tree_lock, 0);	
		}
#endif

		fp->inode = target;
		fp->inode_idx = target->ref;
		target->ufilp[target->ref] = fp;
		target->ref++;
	}
	pthread_mutex_unlock(&uinode_table_lock);

	return target;
}

static void put_inode(uinode *inode, ufile *fp) {
	// Need mutex lock wrap around here
	pthread_mutex_lock(&uinode_table_lock);
	inode->ufilp[fp->inode_idx] = NULL;
	inode->ref--;

	if (inode->ref == 0) {
		uinode_table_delete(inode);
		free(inode);
	}

	pthread_mutex_unlock(&uinode_table_lock);
}


#ifdef PARAFS_INTERVAL_TREE
/*
 * User-level per-inode interval tree function
 */
int crfs_read_submission_tree_search(uinode *inode, unsigned long slba,
				unsigned long nlb, void *buf, ufile **target_fp) {
	int retval = -1;
	struct req_tree_entry *tree_node = NULL;	
	struct interval_tree_node *it;
	unsigned long start = slba;
	unsigned long end = slba + nlb - 1;

        pthread_spin_lock(&inode->sq_tree_lock);
        it = interval_tree_iter_first(&inode->sq_it_tree, start, end);

        if (it) {
                tree_node = container_of(it, struct req_tree_entry, it);
                if (!tree_node) {
                        printf("Get NULL object!\n");
                        retval = -1;
                        goto err_read_submission_tree_search;
                }
        
		/* If a read hit in some other FD-queues, then copy directly */
		memcpy((void*)buf, (const void*)tree_node->blk_addr, tree_node->size);

                *target_fp = tree_node->fp;
                retval = DEVFS_SUBMISSION_TREE_FOUND;

#ifdef PARAFS_STAT
                crfs_stat_fp_queue_hit();
#endif
        } else { 
                retval = DEVFS_SUBMISSION_TREE_NOTFOUND;
        }
        pthread_spin_unlock(&inode->sq_tree_lock);

#ifdef PARAFS_STAT
        crfs_stat_fp_queue_access();
#endif

err_read_submission_tree_search:
        return retval;
}

int crfs_write_submission_tree_search(uinode *inode, unsigned long slba,
					unsigned long nlb, ufile **target_fp) {
	int retval = -1;
	struct req_tree_entry *tree_node = NULL;	
	struct interval_tree_node *it;
	unsigned long start = slba;
	unsigned long end = slba + nlb - 1;

        pthread_spin_lock(&inode->sq_tree_lock);
        it = interval_tree_iter_first(&inode->sq_it_tree, start, end);

        if (it) {
                tree_node = container_of(it, struct req_tree_entry, it);
                if (!tree_node) {
                        printf("Get NULL object!\n");
                        retval = -1;
                        goto err_write_submission_tree_search;
                }
       
                *target_fp = tree_node->fp;
                retval = DEVFS_SUBMISSION_TREE_FOUND;

        } else { 
                retval = DEVFS_SUBMISSION_TREE_NOTFOUND;
        }
        pthread_spin_unlock(&inode->sq_tree_lock);

err_write_submission_tree_search:
        return retval;
}

int crfs_write_submission_tree_insert(uinode *inode, nvme_command_rw_t *cmdrw,
					ufile *fp) {
	int retval = 0;
	struct req_tree_entry *new_tree_node = NULL;	

#ifdef PARAFS_SHM
	new_tree_node = mm_malloc(shm, sizeof(struct req_tree_entry));
#else
	new_tree_node = malloc(sizeof(struct req_tree_entry));
#endif
	if (!new_tree_node) {
		printf("Failed to allocate interval tree node\n");
		retval = -1;
		goto err_submission_tree_insert;
	}

	new_tree_node->blk_addr = (void*)cmdrw->blk_addr;
	new_tree_node->size = cmdrw->nlb;
	new_tree_node->fp = fp;

	new_tree_node->it.start = cmdrw->slba;
	new_tree_node->it.last  = cmdrw->slba + cmdrw->nlb - 1;

	pthread_spin_lock(&inode->sq_tree_lock);
	interval_tree_insert(&new_tree_node->it, &inode->sq_it_tree);
	pthread_spin_unlock(&inode->sq_tree_lock); 

err_submission_tree_insert:
	return retval;
}

int crfs_write_submission_tree_delete(uinode *inode, nvme_command_rw_t *cmdrw) {
	int retval = 0;
	struct req_tree_entry *tree_node = NULL;	
	struct interval_tree_node *it;
	unsigned long start = cmdrw->slba;
	unsigned long end = cmdrw->slba + cmdrw->nlb - 1;

	pthread_spin_lock(&inode->sq_tree_lock);
	it = interval_tree_iter_first(&inode->sq_it_tree, start, end);
	if (it) {
		interval_tree_remove(it, &inode->sq_it_tree);

		tree_node = container_of(it, struct req_tree_entry, it);
		if (tree_node) {
#ifdef PARAFS_SHM
			mm_free(shm, tree_node);
#else
			free(tree_node);
#endif
			tree_node = NULL;
		}
	}
	pthread_spin_unlock(&inode->sq_tree_lock);

	return retval;
}

void search_conflict_write(ufile **target_fp, ufile **origin_fp, ufile **fp,
				unsigned long slba, unsigned long nlb) {
	int search = 0;	

	/* Search for conflict write operation */
	search = crfs_write_submission_tree_search ((*fp)->inode, slba, nlb, target_fp);
	if (search == DEVFS_SUBMISSION_TREE_FOUND) {
		if (*target_fp && *target_fp != *fp) {
#ifdef PARAFS_STAT
			crfs_stat_fp_queue_conflict(); 
#endif
			*origin_fp = *fp;
			*fp = *target_fp;
		}
	}
}
		
int read_from_fd_queue(ufile **target_fp, ufile **origin_fp, ufile **fp,
				unsigned long slba, unsigned long nlb, void* buf) {
	int search = 0, retval = 0;

	/* Search for request in interval tree */
	search = crfs_read_submission_tree_search ((*fp)->inode, slba, nlb, buf, target_fp);
	if (search == DEVFS_SUBMISSION_TREE_FOUND) {
		retval = nlb;

		if (*target_fp && *target_fp != *fp) {
			*origin_fp = *fp;
			*fp = *target_fp;
		}
	}

	return retval;
}

#endif

/*
 * Per FD-queue find and release entry function
 */
#ifndef PARAFS_BYPASS_KERNEL
static void* find_avail_vsq_entry(ufile *fp) {
	int i = 0;
	fd_q *fd_queue = (fd_q*)&fp->fd_queue;
	void *ret = NULL;
	void *vsq = fd_queue->vsq;
	nvme_command_rw_t *cmd = NULL;

	cmd = (nvme_command_rw_t*)(fd_queue->vsq + fd_queue->sq_head);
	if (cmd->status == 0) {
		cmd->status = DEVFS_CMD_READY;
		ret = (void*)cmd;
		fd_queue->sq_head = (fd_queue->sq_head + sizeof(nvme_command_rw_t)) & (fdqueuesize - 1);
	}

	return ret;
}

static void release_vsq_entry(fd_q *fd_queue, void *ioq) {
	nvme_command_rw_t *cmd = (nvme_command_rw_t*)ioq;
	cmd->status = 0;
	return;
}

#else

static void* find_avail_vsq_entry(ufile *fp) {
	int i = 0;
	fd_q *fd_queue = (fd_q*)&fp->fd_queue;
	void *ret = NULL;
	void *vsq = fd_queue->vsq;
	nvme_command_rw_t *cmd = NULL;

	pthread_mutex_lock(&fp->mutex);
find_entry_again:
	cmd = (nvme_command_rw_t*)(fd_queue->vsq + fd_queue->sq_head);
	if (__sync_fetch_and_add(&cmd->status, 0) == 0) {
		ret = (void*)cmd;

	//} else if (__sync_fetch_and_add(&cmd->status, 0) == (DEVFS_CMD_READY | DEVFS_CMD_FINISH)) {
	} else if ((cmd->common.opc == nvme_cmd_write || cmd->common.opc == nvme_cmd_append) && (__sync_fetch_and_add(&cmd->status, 0) == (DEVFS_CMD_READY | DEVFS_CMD_FINISH | DEVFS_CMD_BUSY))) {
		if (cmd->blk_addr) {
#ifdef PARAFS_INTERVAL_TREE
			/* 
			 * As this command has finished by device thread,
			 * remove from interval tree
			 */
			crfs_write_submission_tree_delete(fp->inode, cmd);
#endif                         

#ifdef PARAFS_SHM
			mm_free(shm, (void*)cmd->blk_addr);
#else
			free((void*)cmd->blk_addr);
#endif
		}
		ret = (void*)cmd;
	} else {
		goto find_entry_again;
	}

	memset(cmd, 0, sizeof(nvme_command_rw_t));
	cmd->status |= DEVFS_CMD_BUSY;

	fd_queue->sq_head = (fd_queue->sq_head + sizeof(nvme_command_rw_t)) & (fdqueuesize - 1);
	pthread_mutex_unlock(&fp->mutex);

	return ret;
}

#if 0
static void* find_avail_vsq_entry(ufile *fp) {
	int i = 0;
	fd_q *fd_queue = (fd_q*)&fp->fd_queue;
	void *ret = NULL;
	void *vsq = fd_queue->vsq;
	nvme_command_rw_t *cmd = NULL;
	int offset = fd_queue->sq_head;

	pthread_mutex_lock(&fp->mutex);
find_entry_again:
	cmd = (nvme_command_rw_t*)(fd_queue->vsq + offset);
	if (__sync_fetch_and_add(&cmd->status, 0) == 0) {
		ret = (void*)cmd;

	//} else if (__sync_fetch_and_add(&cmd->status, 0) == (DEVFS_CMD_READY | DEVFS_CMD_FINISH)) {
	} else if ((cmd->common.opc == nvme_cmd_write || cmd->common.opc == nvme_cmd_append) && (__sync_fetch_and_add(&cmd->status, 0) == (DEVFS_CMD_READY | DEVFS_CMD_FINISH | DEVFS_CMD_BUSY))) {
		if (cmd->blk_addr) {
#ifdef PARAFS_INTERVAL_TREE
			/*
			 * As this command has finished by device thread,
			 * remove from interval tree
			 */
			crfs_write_submission_tree_delete(fp->inode, cmd);
#endif

#ifdef PARAFS_SHM
			mm_free(shm, (void*)cmd->blk_addr);
#else
			free((void*)cmd->blk_addr);
#endif
		}
		ret = (void*)cmd;
	} else {
		offset = (offset + sizeof(nvme_command_rw_t)) & (fdqueuesize - 1);
		goto find_entry_again;
	}

	memset(cmd, 0, sizeof(nvme_command_rw_t));
	cmd->status |= DEVFS_CMD_BUSY;

	if (cmd == (nvme_command_rw_t*)(fd_queue->vsq + fd_queue->sq_head))
		fd_queue->sq_head = (fd_queue->sq_head + sizeof(nvme_command_rw_t)) & (fdqueuesize - 1);
	pthread_mutex_unlock(&fp->mutex);

	return ret;
}
#endif

static void release_vsq_entry(fd_q *fd_queue, void *ioq) {
	return;
}
#endif	//PARAFS_BYPASS_KERNEL


/*
 *  DevFS open file
 */
int vfio_crfs_open_filep(int dev, const char* fname,
		int oflags, mode_t mode, int journ, void* qaddr) {

	int ret;
	struct vfio_crfs_creatfp_cmd map;

	map.argsz = sizeof(map);
	strcpy(map.fname, fname);
	map.mode = mode; //0666;
	map.flags = oflags; //(O_CREAT | O_RDWR);
	map.isjourn = journ;
	map.iskernelio = 0;

	map.qentrycnt = qentrycount;
	map.isrdqueue = 0;
	map.vaddr = (u64)qaddr;

	ret = ioctl(dev, VFIO_DEVFS_CREATFILE_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_CREATFILE_CMD for %s  "
				"failed errno %d \n", fname, errno);
		return -errno;
	}
#if defined(_DEBUG)
	fprintf(stderr,"ioctl file open file descriptor %d \n", map.fd);
#endif
	return map.fd;
}

#ifndef PARAFS_BYPASS_KERNEL
/*
 *  DevFS close file
 */
int vfio_crfs_close_file(int dev, int fd) {

	int ret = 0;
	struct vfio_crfs_closefd_cmd map;

#if defined(_DEBUG)
	fprintf(stderr,"vfio_crfs_close_file \n");
#endif
	map.argsz = sizeof(map);
	map.fd = fd;

	ret = ioctl(dev, VFIO_DEVFS_CLOSEFILE_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_CLOSEFILE_CMD errno %d \n", errno);
		return -1;
	}
	return ret;
}
#else
int vfio_crfs_close_file(int dev, int fd) {
	int ret = 0;
#if 0 //def CRFS_OPENCLOSE_OPT
	struct vfio_crfs_closefd_cmd map;

#if defined(_DEBUG)
	fprintf(stderr,"vfio_crfs_close_file \n");
#endif
	map.argsz = sizeof(map);
	map.fd = fd;

	ret = ioctl(dev, VFIO_DEVFS_CLOSEFILE_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_CLOSEFILE_CMD errno %d \n", errno);
		return -1;
	}
	return ret;
#endif

	int opc = nvme_cmd_close;
	void *ioq = NULL;

	ufile *fp = &ufile_table.open_files[fd];
	if (!fp) {
		fprintf(stderr,"failed to get fs libary file pointer \n");
		return -1;
	}

	ioq = find_avail_vsq_entry(fp);
	nvme_cmd_rw_new(opc, CID, NSID, 0, 0, 0, ioq);

	ret = vfio_crfs_queue_write(dev, fd, ioq, cmd_nr);
	release_vsq_entry(&fp->fd_queue, ioq);

	return ret;
}
#endif

#ifndef PARAFS_BYPASS_KERNEL
int vfio_crfs_fsync(int fd) {
	int ret;
	struct vfio_crfs_fsync_cmd map;

	map.argsz = sizeof(map);
	map.fd = fd;

	ret = ioctl(g_dev, VFIO_DEVFS_FSYNC_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_FSYNC_CMD for %d  "
				"failed errno %d \n", fd, errno);
		return -errno;
	}

	return 0;
}
#else
int vfio_crfs_fsync(int fd) {
	int ret, i;
	int opc = nvme_cmd_flush;
	void *ioq = NULL;
	nvme_command_rw_t *cmdrw = NULL;

	ufile *fp = &ufile_table.open_files[fd];
	ufile *cur_fp = NULL;
	uinode *inode = fp->inode;

#ifdef PARAFS_FSYNC_ENABLE
	if (__sync_lock_test_and_set(&inode->fsync_barrier, 1) == 0) {
		/* Traverse FD-queue list of this inode */
		pthread_mutex_lock(&uinode_table_lock);
		inode->fsync_counter = inode->ref;
		pthread_mutex_unlock(&uinode_table_lock);

		for (i = 0; i < MAX_FP_PER_INODE; ++i) {
			cur_fp = inode->ufilp[i];

			if (cur_fp == NULL)
				continue;

			if (__sync_fetch_and_add(&cur_fp->closed, 0) == 1) {
				__sync_fetch_and_sub(&inode->fsync_counter, 1);
				continue;
			}
#if 0
			/* Enqueue fsync cmd of this file pointer */
			ioq = find_avail_vsq_entry(cur_fp);
			nvme_cmd_rw_new(opc, CID, NSID, 0, 0, 0, ioq);

			/* Mark fsync command as ready */
			cmdrw = (nvme_command_rw_t*)ioq;
			//__sync_lock_test_and_set(&cmdrw->status, DEVFS_CMD_READY);
			cmdrw->status |= DEVFS_CMD_READY;
#endif
		}

		__sync_lock_test_and_set(&inode->fsync_barrier, 0);
		inode->fsync_counter = 0;

	} else {
		while (__sync_fetch_and_add(&inode->fsync_barrier, 0) == 1);
	}

	/* loop until fsync barrier flag is cleared */
	while (__sync_fetch_and_add(&inode->fsync_barrier, 0) == 1);
#endif

	return 0;
}
#endif

/**
 * DevFS Submit a read/write command that may require multiple I/O submissions
 * and processing some completions.
 * @param   fd          file descriptor
 * @param   ioq         io queue
 * @param   opc         op code
 * @param   buf         data buffer
 * @param   slba        starting lba
 * @param   nlb         number of logical blocks
 * @return  0 if ok else error status.
 */
int unvme_do_crfs_io(int dev, int dfd, void *ioqq, 
		int opc, void* buf, u64 slba, u64 nlb)
{
	size_t ret = -1;
	void *ioq = NULL;
	int fd = dfd;
	ufile *fp = NULL, *origin_fp = NULL, *target_fp = NULL;
	nvme_command_rw_t *cmdrw = NULL;

#ifdef TEMP_SCALE

#ifndef _USE_OPT
	int tid = (int)syscall(SYS_gettid);
#else
	int tid = ((int)pthread_self() & 0xFFF);
#endif

	ufile *ufp = &ufile_table.open_files[dfd];

	if (ufp->tid != tid &&
		(opc == nvme_cmd_read ||
		 opc == nvme_cmd_write)) {
		/* 
		 * Current thread is not the one that opens
		 * this fd, then it has its own fd for this
		 * thread and all read issued by this thread
		 * will direct to its own fd queue
		 */

		/*
		 * dfd is the origin fd specified by application
		 * dfd*FD_CONFLICT_FACTOR + tid is the index of 
		 * tid_to_fd table to locate the real fd of the
		 * calling thread
		 *
		 * The reason to use dfd*FD_CONFLICT_FACTOR + tid
		 * is to avoid conflict index in tid_to_fd table
		 */
		fd = ufile_table.tid_to_fd[dfd*FD_CONFLICT_FACTOR + tid];

		/*
		 * If the fd for this thread does not exit,
		 * just open for it and then record it to
		 * the tid_to_fd table so that next time
		 * we can locate this fd via dfd and tid
		 */
		if (fd == -1) {
			fd = crfs_open_file(ufp->fname, ufp->perm, ufp->mode);
			if (fd < 0) {
				printf("err crfs_open_file\n");
				exit(-1);
			}
			ufile_table.tid_to_fd[dfd*FD_CONFLICT_FACTOR + tid] = fd;
#ifdef SHADOW_FD
			ufp->shadow_fd[ufp->shadow_fd_nr++] = fd;
#endif
		}
	}
#endif	// TEMP_SCALE

	fp = &ufile_table.open_files[fd];
	origin_fp = fp;

#ifdef PARAFS_BYPASS_KERNEL
#ifdef PARAFS_INTERVAL_TREE
	if (opc == nvme_cmd_write || opc == nvme_cmd_append) {
		/* Search for conflict write operations */
		search_conflict_write(&target_fp, &origin_fp, &fp, slba, nlb);	

		if (origin_fp != fp && __sync_lock_test_and_set(&fp->closed_conflict, 1) == 1) {
			fp = origin_fp;
		}

		/* Get the submission head of FD-queue */
		ioq = find_avail_vsq_entry(fp);
		nvme_cmd_rw_new(opc, CID, NSID, slba, nlb, (u64)buf, ioq);
		cmdrw = (nvme_command_rw_t*)ioq;

		/* When conflict has resolved, mark it ready to close */
		__sync_lock_test_and_set(&fp->closed_conflict, 0);

		/* Allocate buffer for the write/append */
#ifdef PARAFS_SHM
		cmdrw->blk_addr = (__u64)mm_malloc(shm, nlb);
#else
		cmdrw->blk_addr = (__u64)malloc(nlb);
#endif
		memcpy((void*)cmdrw->blk_addr, (const void*)buf, nlb);

		/* Add current write request to interval tree */        
		if (crfs_write_submission_tree_insert(fp->inode, cmdrw, fp)) {
			printf("interval tree insertion fail\n");
			return -1;
		}

		/* Mark this request as ready, then write is done :) */
		//__sync_lock_test_and_set(&cmdrw->status, DEVFS_CMD_READY);
		cmdrw->status |= DEVFS_CMD_READY;

		return nlb;

	} else if (opc == nvme_cmd_read) {
		/* read operation fast path */
		if (read_from_fd_queue(&target_fp, &origin_fp, &fp, slba, nlb, buf) == nlb)
			return nlb;
	}
#else
	if (opc == nvme_cmd_write || opc == nvme_cmd_append) {
		/* Get the submission head of FD-queue */
		ioq = find_avail_vsq_entry(fp);
		nvme_cmd_rw_new(opc, CID, NSID, slba, nlb, (u64)buf, ioq);
		cmdrw = (nvme_command_rw_t*)ioq;

		/* Allocate buffer for the write/append */
#ifdef PARAFS_SHM
		cmdrw->blk_addr = (__u64)mm_malloc(shm, nlb);
#else
		cmdrw->blk_addr = (__u64)malloc(nlb);
#endif
		memcpy((void*)cmdrw->blk_addr, (const void*)buf, nlb);

		/* Mark this request as ready, then write is done :) */
		//__sync_lock_test_and_set(&cmdrw->status, DEVFS_CMD_READY);
		cmdrw->status |= DEVFS_CMD_READY;

		return nlb;
	}
#endif //PARAFS_INTERVAL_TREE
#endif //PARAFS_BYPASS_KERNEL

#ifndef PARAFS_BYPASS_KERNEL
#ifndef TEMP_SCALE
	pthread_mutex_lock(&fp->mutex);
	while (!(ioq = find_avail_vsq_entry(fp)));
	pthread_mutex_unlock(&fp->mutex);
#else
	ioq = find_avail_vsq_entry(fp);
#endif
#else
	ioq = find_avail_vsq_entry(fp);
#endif

	// Create new cmd for this request
	nvme_cmd_rw_new(opc, CID, NSID, slba, nlb, (u64)buf, ioq);

	// Do the actual I/O
	ret = vfio_crfs_queue_write(dev, fd, ioq, cmd_nr);

	// Release entry
	release_vsq_entry(&fp->fd_queue, ioq);

	return ret;
}

#ifdef _NVMFDQ
int global_nvm_id = 0;
pthread_mutex_t nvlock = PTHREAD_MUTEX_INITIALIZER;

void *align_addr(int req_size, char *fname) {

	void *bufAddr = NULL;
	unsigned long temp = 0;
	bufAddr = (char *)p_c_nvalloc_(req_size, fname, ((int)syscall(SYS_gettid))+ global_nvm_id);
	global_nvm_id++;
	temp = (unsigned long)bufAddr;
	temp = (temp & ~(PAGE_SIZE-1)) + PAGE_SIZE; 
	bufAddr = (void *)temp;
	memset(bufAddr,0,req_size);
	//fprintf(stderr, "finishing memset \n");
	return bufAddr;
}
#endif

void* vfio_queue_get_buffer(int dev, unsigned int num_pages) {

	void *bufAddr;
	int ret;
	if(num_pages == 0) return NULL;

	int req_size = (PAGE_SIZE * (num_pages+1));

#ifdef PARAFS_SHM
	bufAddr = mm_malloc(shm, req_size);
#else
	posix_memalign(&bufAddr, PAGE_SIZE, req_size);
	mlock(bufAddr, req_size);
#endif
	if(bufAddr == NULL) {
		fprintf(stderr,"vfio_queue_get_buffer buffer create failed \n");
		return NULL;
	}

	memset(bufAddr, 0, req_size);
	struct vfio_iommu_type1_queue_map map = {
			.argsz = sizeof(map),
			.flags = (VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE),
			.vaddr = (__u64)bufAddr,
			.iova = (__u64)0, //dev->iova,
			.size = (__u64)0,
			.qpfnlist = bufAddr + (PAGE_SIZE * num_pages) + 8,
			.qpfns = (__u64)num_pages,
	};

	if (ioctl(dev, VFIO_IOMMU_GET_QUEUE_ADDR, &map) < 0) {
		fprintf(stderr,"ioctl VFIO_IOMMU_GET_QUEUE_ADDR errno %d \n", errno);
#ifdef PARAFS_SHM
		mm_free(shm, bufAddr);
#else
		free(bufAddr);
#endif
		return NULL;
	}

	struct vfio_crfs_creatfs_cmd fsmap = {
			.argsz = sizeof(fsmap),
			.flags = (VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE),
			.vaddr = 0,
			.iova = 0, // dev->iova,
			.nblocks = num_pages,
			.size = (__u64)(PAGE_SIZE * num_pages),
			.dev_core_cnt = (__u32)devcorecnt,
			.sched_policy = (__u32)schedpolicy,
	};

	if (ioctl(dev, VFIO_DEVFS_CREATFS_CMD, &fsmap) < 0) {
		fprintf(stderr,"VFIO_DEVFS_CREATFS_CMD errno %d \n", errno);

		exit(-1);
		return NULL;
	}

	/* Store the credential id get from OS */
	memcpy(cred_id, fsmap.cred_id, CRED_ID_BYTES);

	return bufAddr;
}

static void* fd_queue_alloc(unsigned int num_pages) {
	void *bufAddr = NULL;
	/*pthread_mutex_lock(&fd_q_mempool.lock);
	while (((fd_q_mempool.head + PAGE_SIZE) % (FD_QUEUE_POOL_SIZE - 1)) == fd_q_mempool.tail);
	bufAddr = fd_q_mempool.mem + fd_q_mempool.head;
	fd_q_mempool.head = (fd_q_mempool.head + PAGE_SIZE) % (FD_QUEUE_POOL_SIZE - 1);
	pthread_mutex_unlock(&fd_q_mempool.lock);*/

	pthread_mutex_lock(&fd_q_mempool.lock);
	while (fd_q_mempool.bitmap[fd_q_mempool.head] != 0)
		fd_q_mempool.head = (fd_q_mempool.head + 1) & (FD_QUEUE_POOL_PG_NUM - 1);
	fd_q_mempool.bitmap[fd_q_mempool.head] = 1;
	bufAddr = fd_q_mempool.mem + fd_q_mempool.head * PAGE_SIZE;
	fd_q_mempool.head = (fd_q_mempool.head + 1) & (FD_QUEUE_POOL_PG_NUM - 1);
	pthread_mutex_unlock(&fd_q_mempool.lock);

	return bufAddr;
}

static void fd_queue_free(void *q_addr) {
	int idx = (q_addr - fd_q_mempool.mem) >> 12;
	/*pthread_mutex_lock(&fd_q_mempool.lock);
	fd_q_mempool.tail = (fd_q_mempool.tail + PAGE_SIZE) % (FD_QUEUE_POOL_SIZE - 1);	
	pthread_mutex_unlock(&fd_q_mempool.lock);*/
	fd_q_mempool.bitmap[idx] = 0;
}

void* vfio_get_fd_queue_buffer(unsigned int num_pages, char *fname) {

	void *bufAddr = NULL;
	int ret;
	if (num_pages == 0)
		return NULL;

#ifdef CRFS_OPENCLOSE_OPT
	bufAddr = fd_queue_alloc(num_pages);
	if (bufAddr == NULL) {
		fprintf(stderr,"vfio_queue_get_buffer buffer create failed \n");
		return NULL;
	}

	return bufAddr;
#endif

#ifdef _NVMFDQ
	int req_size = (PAGE_SIZE * (num_pages + 5));
	pthread_mutex_lock(&nvlock);
	bufAddr = align_addr(req_size, (char *)fname);
	pthread_mutex_unlock(&nvlock);
#else
	int req_size = (PAGE_SIZE * num_pages);
        posix_memalign(&bufAddr, PAGE_SIZE, req_size);
#endif

	if (bufAddr == NULL) {
		fprintf(stderr,"vfio_queue_get_buffer buffer create failed \n");
		return NULL;
	}
#ifndef _NVMFDQ
	memset(bufAddr, 0, req_size);
#endif
	mlock(bufAddr, req_size);

	return bufAddr;
}

void vfio_put_fd_queue_buffer(void* q_addr) {
#ifndef _NVMFDQ
#ifdef PARAFS_SHM
        mm_free(shm, fp->fd_queue.vsq);
#else

#ifndef CRFS_OPENCLOSE_OPT
        free(q_addr);
#else
	fd_queue_free(q_addr);
#endif

#endif
#endif
}

void set_realtime_priority() {
	int ret;

	// We'll operate on the currently running thread.
	pthread_t this_thread = pthread_self();
	struct sched_param params;
	params.sched_priority = sched_get_priority_max(SCHED_FIFO);                
	ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
	if (ret != 0) {
		return; 
	}
}

int setaffinity() {
	int s, j;
	cpu_set_t cpuset;
	pthread_t thread = pthread_self();
	assert(thread);
	/* Set affinity mask to include CPUs 3 to 8 */
	CPU_ZERO(&cpuset);
	for (j = 0; j < 10; j++) 
		CPU_SET(j, &cpuset);
	s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (s != 0) { 
		fprintf(stderr, "failed pthread_setaffinity_np");
		return -1;
	}    
	return 0;
}

void* run_dev_thread(void* arg) {
	struct vfio_crfs_init_devthread_cmd map = {
			.argsz = sizeof(map),
			.dev_core_cnt = (__u32)devcorecnt,
			.thread_index = (__u32)*(int*)arg,
			.sched_policy = (__u32)schedpolicy,
        };

	//set_realtime_priority();
#ifdef REDUCE_CPU_FREQ
	setaffinity();
#endif
	if (ioctl(g_dev, VFIO_DEVFS_INIT_DEVTHREAD_CMD, &map) < 0) {
		fprintf(stderr,"VFIO_DEVFS_INIT_DEVTHREAD_CMD errno %d \n", errno);
	}
	return NULL;
}


int initialize_crfs(unsigned int qentry_count,
		     unsigned int dev_core_cnt,
		     unsigned int sched_policy) {
	u64 datasize = 4096;
	u64* p = NULL;
	int q = 0, fd = -1;
	int perm = CREATDIR, i = 0;
#ifdef PARAFS_SCHED_THREAD
	pthread_t tid;
#endif

#ifdef _NVMFDQ
	nvinit(getpid());
#endif

#ifdef PARAFS_SHM
	shm = mm_create(SHM_SIZE, SHM_POOL);
	if (!shm) {
		printf("failed to allocate shared memory!\n");
		exit(1);
	}

	ufile_table_ptr = malloc(sizeof(struct open_file_table));
#endif

	// Set qentrycount, devcorecnt and schedpolicy
	if (qentry_count)
		qentrycount = qentry_count;
	if (dev_core_cnt)
		devcorecnt = dev_core_cnt;
	if (sched_policy)
		schedpolicy = sched_policy;

	g_dev = open(TEST, CREATDIR, MODE);

	if(g_dev == -1){
		printf("Error!");
		exit(1);
	}

	//vsq = vfio_queue_get_buffer(g_dev, 1);
	vsq = vfio_queue_get_buffer(g_dev, FD_QUEUE_POOL_PG_NUM);
	//g_vsq = vsq;
	fd_q_mempool.mem = vsq;
	memset(fd_q_mempool.bitmap, 0, FD_QUEUE_POOL_PG_NUM*sizeof(int));
	fd_q_mempool.head = 0;
	pthread_mutex_init(&fd_q_mempool.lock, NULL);

#ifdef TEMP_SCALE
	memset(ufile_table.tid_to_fd, -1, MAX_THREAD_NR*sizeof(int));
#endif
	/* Initialize user level inode table */
	//memset(ufile_table.open_inodes, 0, MAX_OPEN_INODE*sizeof(uinode));
	memset(ufile_table.open_files, 0, MAX_OPEN_INODE*sizeof(ufile));
	//pthread_mutex_init(&ufile_table.inode_table_lock, NULL);

#ifdef PARAFS_SCHED_THREAD
	/*
	 * If using pthread as device thread, just call
	 * run_dev_thread() to initialize
	 */
	for (i = 0; i < devcorecnt; ++i) {
		pthread_create(&tid, NULL, &run_dev_thread, &i);
		//pthread_detach(tid);
		sleep(1);
	}
#endif

//#ifdef _USE_OPT
	//setHandler(fault_handler);	
//#endif

	return 0;
}


int shutdown_crfs()
{
	int ret;
	struct vfio_crfs_closefs_cmd map;

	map.argsz = sizeof(map);
	memcpy(map.cred_id, cred_id, CRED_ID_BYTES);

	ret = ioctl(g_dev, VFIO_DEVFS_CLOSEFS_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_CLOSEFS_CMD for %s  "
				"failed errno %d \n", fname, errno);
		return -errno;
	}

	/*if (fd_q_mempool.mem)
		free(fd_q_mempool.mem);*/

	isexit = 1;

#ifdef PARAFS_STAT
	crfs_stat_fp_queue_count(); 
#endif

	return 0;
}


/****************************************************************
 * Entry function for each POSIX I/O syscalls for DevFS
 * *************************************************************/

/*
 * Open file from crfs
 */
int crfs_open_file(const char *fname, int perm, mode_t mode)
{
	int fd = -1;
	
	ufile *fp = NULL;
	uinode *inode = NULL;

	g_slba = 0;

	if(!qentrycount)
		qentrycount = 1;

	// Allocate 1 page for command buffer
	void *qaddr = vfio_get_fd_queue_buffer(FD_QUEUE_PAGE_NUM, (char *)fname);

	fd = vfio_crfs_open_filep(g_dev, (char *)fname, perm, mode, isjourn, qaddr);
	if(fd < 0) {
		fprintf(stderr,"crfs_open_file failed %d \n", errno);
		return fd;
	}

	/* Setup user-level file pointer */
	fp = &ufile_table.open_files[fd];

	/* Wait until this user-level file pointer is released by another thread */
	while (fp->fd > 0);

	fp->fd = fd;
	fp->ref = 1;
	fp->off = 0;
	fp->fd_queue.vsq = qaddr;
	fp->fd_queue.sq_head = 0;
	fp->fd_queue.size = fdqueuesize;

	/* Setup user-level inode */
	inode = get_inode(fname, fp);	
	if (inode == NULL) {
		fprintf(stderr,"uinode not found\n");
		exit(-1);
	}
	// If fsync barrier is set, should spin here
#ifdef PARAFS_FSYNC_ENABLE
	/* loop until current fsync is done */
	while (__sync_fetch_and_add(&inode->fsync_barrier, 0) == 1);
#endif

	/* Setup FD-queue */
	pthread_mutex_init(&fp->mutex, NULL);
	pthread_cond_init(&fp->cond, NULL);

#ifdef TEMP_SCALE
	memcpy(fp->fname, fname, strlen(fname));
	fp->perm = perm;
	fp->mode = mode;
#ifndef _USE_OPT
	fp->tid = syscall(SYS_gettid);
#else
	fp->tid = ((int)pthread_self() & 0xFFFF);
#endif
#endif
	fp->closed = 0;
	fp->closed_conflict = 0;

	return fd;
}


/*
 * Open file from crfs
 */
int crfs_close_file(int fd)
{
	ufile *fp = NULL;
	uinode *inode = NULL;

	int numclose = 0;

	if(fd < 0) {
		fprintf(stderr,"crfs_open_file failed %d \n", errno);
	}

	fp = &ufile_table.open_files[fd];
	inode = fp->inode;

#ifdef PARAFS_INTERVAL_TREE
	/* Wait until conflict case is resolved before close */
	while (__sync_lock_test_and_set(&fp->closed_conflict, 1) == 1);
#endif

#ifdef PARAFS_FSYNC_ENABLE
	/* loop until current fsync is done */
	while (__sync_fetch_and_add(&inode->fsync_barrier, 0) == 1);

	/* mark this fp is being closed, do not insert fsync to this FD-queue */
	__sync_lock_test_and_set(&fp->closed, 1);
#endif

#ifdef SHADOW_FD
        ufile *shadow_fp = NULL;
        int shadow_fd = 0;
	numclose = fp->shadow_fd_nr;

        for (int i = 0; i < numclose; ++i) {
                shadow_fd = fp->shadow_fd[i];
                shadow_fp = &ufile_table.open_files[shadow_fd];
                shadow_fp->fd = -1;
                shadow_fp->ref = 0;
                shadow_fp->off = -1;
		shadow_fp->closed = 0;

                shadow_fd = vfio_crfs_close_file(g_dev, shadow_fd);
                if(shadow_fd < 0) {
                        fprintf(stderr,"crfs_open_file failed %d \n", errno);
                }
#ifndef _NVMFDQ
#ifdef PARAFS_SHM
		mm_free(shm, shadow_fp->fd_queue.vsq);
#else
		vfio_put_fd_queue_buffer(fp->fd_queue.vsq);
#endif
#endif
		fp->shadow_fd_nr--;
        }
        fp->shadow_fd_nr = 0;
#endif

	if (vfio_crfs_close_file(g_dev, fd) < 0) {
		fprintf(stderr,"crfs_close_file failed %d \n", errno);
	}

	__sync_lock_test_and_set(&fp->closed, 0);

	/* Remove user-level file pointer in user-level inode */
	put_inode(inode, fp);

	/* Free fd-queue buffer */
	vfio_put_fd_queue_buffer(fp->fd_queue.vsq);

	fp->fd = -1;
	fp->ref = 0;
	fp->off = -1;

	return 0;
}

/*Append write to existing file position*/
size_t crfs_write(int fd, const void *p, size_t count) {

	u64 nlb = 0;
	size_t write_cnt = 0, left = count;
	int written = 0;
	int retval;

	ufile *fp = &ufile_table.open_files[fd];
	if (!fp) {
		printf("failed to get ufile %d\n", fd);
		return -1;
	}

#if defined(_DEBUG)
	printf("crfs_write writing at %zu offset \n", count);
#endif

	nlb = fp->off;

	retval = unvme_do_crfs_io(g_dev, fd, fp->fd_queue.vsq, nvme_cmd_append, (void *)p, nlb, (u64)count);
	if (retval < 0){
		printf("crfs_write failed %d\n", retval);
		return -1;
	}

	fp->off += retval;

#if defined(_DEBUG)
	printf("crfs_write wrote at %zu retval \n", retval);
#endif
	return retval;
}

size_t crfs_pwrite(int fd, const void *p, size_t count, off_t offset) {

	u64 nlb = 0;
	size_t write_cnt = 0, left = count;
	int written = 0;
	int retval;

	ufile *fp = &ufile_table.open_files[fd];
	if (!fp) {
		printf("failed to get ufile %d\n", fd);
		return -1;
	}

	nlb = offset;

#if defined(_DEBUG)
	printf("crfs_write writing at %zu offset \n", count);
#endif

	retval = unvme_do_crfs_io(g_dev, fd, fp->fd_queue.vsq, nvme_cmd_write, (void *)p, nlb, (u64)count);
	if (retval < 0){
		printf("crfs_write failed %d\n", retval);
		return -1;
	}

#if defined(_DEBUG)
	printf("crfs_write wrote at %zu retval \n", retval);
#endif
	return (size_t)retval;
}


/*Append write to existing file position*/
size_t crfs_read(int fd, void *p, size_t count) {

	u64 nlb = 0;
	int retval;

	ufile *fp = &ufile_table.open_files[fd];
	if (!fp) {
		printf("failed to get ufile %d\n", fd);
		return -1;
	}

	// mark as -1 to indicate this is a read call
	// instead of a pread
	nlb = INVALID_SLBA;

#if defined(_DEBUG)
	printf("***crfs_read reading at %zu offset \n", count);
	printf("***crfs_read unvme_do_crfs_io called \n");	
#endif

	retval = unvme_do_crfs_io(g_dev, fd, fp->fd_queue.vsq, nvme_cmd_read, p, nlb, (u64)count);
	if (retval < 0){
		printf("crfs_read failed fd = %d, pos = %lu, count = %lu \n", fd, nlb, count);
		return -1;
	}

	//printf("count = %d, offset = %d\n", count, fp->off);

	fp->off += retval;

#if defined(_DEBUG)
	printf("crfs_read at %zu retval \n", retval);
#endif
	return (size_t)retval;
}

size_t crfs_pread(int fd, void *p, size_t count, off_t offset) {

	u64 nlb = 0;
	int retval;

	ufile *fp = &ufile_table.open_files[fd];
	if (!fp) {
		printf("failed to get ufile %d\n", fd);
		return -1;
	}

	nlb = offset;

#if defined(_DEBUG)
	printf("***crfs_read reading at %zu offset \n", count);
	printf("***crfs_read unvme_do_crfs_io called \n");	
#endif
	
	retval = unvme_do_crfs_io(g_dev, fd, fp->fd_queue.vsq, nvme_cmd_read, p, nlb, (u64)count);
	if (retval < 0) {
		printf("crfs_pread failed fd = %d, pos = %lu, count = %lu \n", fd, nlb, count);
		return -1;
	}

#if defined(_DEBUG)
	printf("crfs_read at %zu retval \n", retval);
#endif
	return (size_t)retval;
}


int crfs_lseek64(int fd, off_t offset, int whence) {

	u64 slba = whence;
	int retval;

	ufile *fp = &ufile_table.open_files[fd];
	if (!fp) {
		printf("failed to get ufile %d\n", fd);
		return -1;
	}

	if (whence == SEEK_SET) {
		fp->off = offset;
	} else if (whence == SEEK_CUR) {
		fp->off += offset;
	} else {
		//TODO
	}

	retval = fp->off;

	/* 
	 * Since we offload file offset managing in user space
	 * we don't have to send io command any more
	 */

	/*retval = unvme_do_crfs_io(g_dev, fd, g_vsq, nvme_cmd_lseek, NULL, slba, (u64)offset);
	return (size_t)offset;
	if (retval < 0){
		printf("crfs_lseek64 failed %d\n", retval);
		return -1;
	}*/

#if defined(_DEBUG)
	printf("crfs_write read at %zu retval \n", retval);
#endif
	return (size_t)retval;
}

int crfs_fsync(int fd)
{
	int ret;
	struct vfio_crfs_fsync_cmd map;

	map.argsz = sizeof(map);
	map.fd = fd;

	ret = vfio_crfs_fsync(fd);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_FSYNC_CMD for %d  "
				"failed errno %d \n", fd, errno);
		return -errno;
	}

	return 0;
}

int crfs_fallocate(int fd, off_t offset, off_t len)
{
        return 0;
}

int crfs_ftruncate(int fd, off_t length)
{
        return 0;
}

int crfs_unlink(const char *pathname)
{
	int ret;
	struct vfio_crfs_unlink_cmd map;

	map.argsz = sizeof(map);
	map.uptr = (u64)pathname;

	ret = ioctl(g_dev, VFIO_DEVFS_UNLINK_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_UNLINK_CMD for %s  "
				"failed errno %d \n", pathname, errno);
		return -errno;
	}

	return 0;

}

int crfs_rename(const char *oldpath, const char *newpath)
{
	int ret;
	struct vfio_crfs_rename_cmd map;

	map.argsz = sizeof(map);
	map.oldname = (u64)oldpath;
	map.newname = (u64)newpath;

	ret = ioctl(g_dev, VFIO_DEVFS_RENAME_CMD, &map);
	if (ret < 0) {
		fprintf(stderr,"ioctl VFIO_DEVFS_UNLINK_CMD for %s %s "
				"failed errno %d \n", oldpath, newpath, errno);
		return -errno;
	}

	return 0;

}

