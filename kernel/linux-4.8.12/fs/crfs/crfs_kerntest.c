/*
 * DevFS: Loadable Device File System
 *
 * Copyright (C) 2017 Sudarsun Kannan.  All rights reserved.
 *     Author: Sudarsun Kannan <sudarsun.kannan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Derived from original ramfs:
 * TODO: DEVFS description
 */

/*TODO: Header cleanup*/
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/file.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/nvme.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/fsnotify.h>
#include <linux/mount.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include <linux/snappy.h>
#include <linux/time.h>



#define DEVFS_BASE_NAME "/mnt/ram/"
#define DEVFS_MAX_IO 16*1024*1024*1024L
#define DEVFS_BLOCKS 4
#define _DEVFS_THREAD_AFFNITY 4
#define _DEVFS_IO_ENABLE
#define NUM_CPUS 4

DECLARE_COMPLETION(comp);
struct task_arg {
   volatile int fd;
   nvme_cmdrw_t *cmdrw;
   unsigned int fno;
   volatile int task_pending;
   volatile int task_finish;
   volatile int cpuid;	
   struct completion *comp;	
};
static struct task_struct **threads;
volatile struct task_arg taskargs[NUM_CPUS];

int crfss_run_test(nvme_cmdrw_t *cmdrw);

static int g_qentrycount;
static int g_curr_cpuid;


/*
 *  DevFS open file
 */
int create_open_filep(const char* fname,
                int oflags, mode_t mode) {

        struct vfio_crfss_creatfp_cmd map;
	//char __user *p = NULL;

	g_qentrycount = 1;
        map.argsz = sizeof(map);
        strcpy(map.fname, fname);
        map.mode = 0666;
        map.flags = (O_CREAT | O_RDWR);
        map.isjourn = 0;
        map.iskernelio = 0;
        map.qentrycnt = g_qentrycount;
        map.isrdqueue = 0;

        //ret = ioctl(dev, VFIO_DEVFS_CREATFILE_CMD, &map);
	//p = (__force char __user *)&map;
        vfio_creatfile_cmd((unsigned long)&map, 1);
        if (map.fd < 0) {
                printk(KERN_ALERT " VFIO_DEVFS_CREATFILE_CMD for %s  "
                                "failed fd %d \n", fname, map.fd);
                return -1;
        }
#if defined(_DEBUG)
        printk("ioctl file open file descriptor %d \n", map.fd);
#endif
        return map.fd;
}



int create_file(int oldfd, const char *suffix) {

	char outname[256];
	char fileno[64];
	int fd = -1;

	strcpy(outname, DEVFS_BASE_NAME);
        snprintf(fileno, 64, "%d", oldfd);
	strcat(outname, fileno); 
	strcat(outname, suffix);

#ifdef _DEBUG
	crfss_dbgv( "%s:%d filename %s \n", 
		__FUNCTION__, __LINE__, outname);
#endif

	fd = create_open_filep((const char *)outname,
		(O_CREAT | O_RDWR), 0666);

#ifdef _DEBUG
	crfss_dbgv( "%s:%d success file create %d name %s \n", 
		__FUNCTION__, __LINE__, fd, outname);
#endif

	return fd;
}

int perform_write(int fd, nvme_cmdrw_t *cmd, u8 opc, 
		struct crfss_fstruct *rd)
{
	u8 append = 1;
	ssize_t retval = 0;

	if ((opc == nvme_cmd_write) || (opc == nvme_cmd_append)) {
#ifdef _DEVFS_DIRECTIO
        	retval =  crfss_direct_write (rd,(void *)cmd, append);
#else
              	retval = rd_write(rd, (void *)cmd, sizeof(nvme_cmdrw_t), fd, append);
#endif
	}
	return retval;
}


#if defined(_DEVFS_SNAPPY_TEST)

#define DEVFS_SNAPPY_FILES 10
#define _DEVFS_SNAPPY_THREADS
struct snappy_env g_env;
static int g_snappy_init;
static int g_thrd_init;
int perform_compress(int fd, nvme_cmdrw_t *cmdrw, unsigned int fno);
void compress_func(void *data);

#ifdef _DEVFS_SNAPPY_THREADS
void compress_func(void *data);
volatile int g_taskwait;
volatile struct task_arg g_task;

nvme_cmdrw_t *g_cmdrw = NULL;
char *g_input = NULL;

struct compress_arg {
   volatile void *p;
   ssize_t isize;
   volatile void *compressd;
   ssize_t *compress_len;
   u64 task_pending;
   u64 task_finish;
}; 

volatile static struct compress_arg g_compress_arg;

__cacheline_aligned_in_smp DEFINE_SPINLOCK(thrdlock);


void compress_thread_func(void *data) {

	//struct compress_arg *arg = (struct compress_arg *)data;
	struct compress_arg *arg = (struct compress_arg *)&g_compress_arg;
	int err = -1;
	const char __user *p = NULL;
	char *input;
	//char __user *compressd;

wait_for_task:
	while(!arg->task_pending) {
		cpu_relax();
		if(g_compress_arg.task_finish)
			return;
		smp_mb();
	}

	spin_lock(&thrdlock);

	BUG_ON(!arg);

	//smp_mb();

	char __user *compressd = (void *)g_compress_arg.compressd;
	BUG_ON(!compressd);


	/*if(!g_input) {
                g_input = kmalloc(arg->isize, GFP_USER);
                memset(g_input, 'a', arg->isize);
                printk(KERN_ALERT "%s:%d Initializing Input size %lu\n",
                                __FUNCTION__, __LINE__, arg->isize);
        }
        input = g_input;*/
	input = vmalloc(arg->isize);
	if(!input) {
		printk(KERN_ALERT "%s, %d Input is NULL \n", __FUNCTION__, __LINE__);
		return;
	}

	//printk(KERN_ALERT " arg->isize %lu \n", arg->isize);
	crfss_dbgv("%s:%d compressd %lu, input %lu g_compress_arg.compressd %lu\n",
			 __FUNCTION__, __LINE__, compressd, input, g_compress_arg.compressd);

 	//memset(input, 'a', arg->isize);
	p = (__force char __user *)input;

	if((err = snappy_compress(&g_env, p, arg->isize, compressd, g_compress_arg.compress_len)) != 0) {
                printk(KERN_ALERT "%s, %d compress failed\n",
                                         __FUNCTION__, __LINE__);
	}

	arg->task_pending = 0;

	spin_unlock(&thrdlock);

	////kfree(input);
	vfree(input);

	smp_mb();

	crfss_dbgv("%s:%d compress_func success %zu\n",
	          __FUNCTION__, __LINE__, *(arg->compress_len));

	goto wait_for_task;

#if 0
wait_for_task:

	while(! &arg->task_pending) {

		printk(KERN_ALERT "%s:%d, cpuid %d task_pending %d, "
			"task_finish %d \n", __FUNCTION__, __LINE__, 
			cpuid, arg->task_pending, arg->task_finish);
		
		if(g_taskwait) {
			printk(KERN_ALERT "g_taskwait set \n");
			break;
		}
	}
	arg->task_pending = 0;
	arg->task_finish  = 1;

	printk(KERN_ALERT"%s:%d task CPU %d exiting\n",
		 __FUNCTION__, __LINE__, cpuid);
	goto start_compress;
#endif
}




void compress_func(void *data) {

	struct task_arg *arg = (struct task_arg *)data;
	int cpuid = arg->cpuid;
	int fd = -1;
	unsigned int fno;
	nvme_cmdrw_t *cmdrw;

	 printk(KERN_ALERT"%s:%d compress_func called CPUID %d\n",
	          __FUNCTION__, __LINE__, cpuid);

	BUG_ON(!arg);

start_compress:

	if(cpuid < 0 || cpuid > NUM_CPUS) {
		printk(KERN_ALERT"%s:%d Incorrect CPUID %d\n",
			 __FUNCTION__, __LINE__, cpuid);
		return;
	}

wait_for_task:

	while(! &arg->task_pending) {

		printk(KERN_ALERT "%s:%d, cpuid %d task_pending %d, "
			"task_finish %d \n", __FUNCTION__, __LINE__, 
			cpuid, arg->task_pending, arg->task_finish);
		
		if(g_taskwait) {
			printk(KERN_ALERT "g_taskwait set \n");
			break;
		}
	}

	/*if (!taskargs[cpuid]->task_pending) {

	    printk(KERN_ALERT "%s:%d going to sleep %d cpuid\n", 
			__FUNCTION__, __LINE__, cpuid);
	    set_current_state(TASK_INTERRUPTIBLE);
	    schedule();
	}*/

	/* got a task, fill arguments */
	fd = arg->fd;
	cmdrw = arg->cmdrw;        
	fno = arg->fno;

	printk(KERN_ALERT"%s:%d fd %d, cmdrw %lu, fno %u prp2 %lu \n",
		__FUNCTION__, __LINE__, fd, cmdrw, fno, cmdrw->common.prp2);

	if(!cmdrw) {
		
		BUG_ON(!cmdrw);

		printk(KERN_ALERT"%s:%d Failed, cmdrw NULL \n",
			 __FUNCTION__, __LINE__);
		return;
	}

	perform_compress(fd, cmdrw, fno);

	printk(KERN_ALERT"%s:%d perform_compress for CPU %d finished\n",
		 __FUNCTION__, __LINE__, cpuid);

	arg->task_pending = 0;
	arg->task_finish  = 1;

	printk(KERN_ALERT"%s:%d task CPU %d exiting\n",
		 __FUNCTION__, __LINE__, cpuid);

	//goto start_compress;
}


static void free_thrd_resources(int cpus)
{
	int i = 0;
	for (i = 0; i < NUM_CPUS; i++) {		
		if(threads[i]) {
			g_compress_arg.task_finish = 1;
			smp_mb();
			kthread_stop(threads[i]);
			printk(KERN_ALERT "STOPPING THREAD\n");
		}
	}
        kfree(threads);
	spin_unlock(&thrdlock);
	g_snappy_init = 0;
	g_thrd_init = 0;
	spin_lock_init(&thrdlock);
	//kfree(taskargs);
}

static int alloc_thrd_resources(void)
{
        int cpus = NUM_CPUS;
	int i = 0;

        threads = kzalloc(cpus * sizeof(struct task_struct *), GFP_KERNEL);
        if (!threads)
                goto fail;

        /*taskargs = kzalloc(cpus * sizeof(struct tast_arg *), GFP_KERNEL);
        if (!taskargs)
                goto fail;
	for (i = 0; i < cpus; i++) {
        	taskargs[i] = kzalloc(sizeof(struct task_arg), GFP_KERNEL);
	        if (!taskargs[i])
        	        goto fail;
	}*/

	printk(KERN_ALERT"%s:%d thread resource allocation succeeded \n", 
		__FUNCTION__, __LINE__);

        return 0;
fail:
        free_thrd_resources(cpus);
	printk(KERN_ALERT"%s:%d thread allocation or task_arg creation failed\n", 
			__FUNCTION__, __LINE__);
        return -ENOMEM;
}


static int run_thrds(void)
{
        int cpus = NUM_CPUS;
	int i = 0;

	for (i = 0; i < cpus; i++) {
		g_compress_arg.task_pending = 0;
		g_compress_arg.task_finish = 0;
		threads[i] = kthread_run(compress_thread_func, &g_compress_arg, "compress thread");
		BUG_ON(!threads[i]);
		kthread_bind(threads[i], _DEVFS_THREAD_AFFNITY);
	}
#if 0
        for (i = 0; i < cpus; i++) {
		taskargs[i].cpuid = i;
	        threads[i] = kthread_create(compress_func,
       		                    &taskargs[i], "compress thread");
		//BUG_ON(!threads[i]);
	        kthread_bind(threads[i], i);
		if (i == g_curr_cpuid) {	    	
			wake_up_process(threads[i]);
		}
        }
#endif
	printk(KERN_ALERT"%s:%d thread creation done\n", 
			__FUNCTION__, __LINE__);
        return 0;
}

#endif


int test_snappy_init_env(void){
	snappy_init_env(&g_env);
	g_snappy_init = 1;
	g_qentrycount = 1;	

#ifdef _DEVFS_SNAPPY_THREADS
	//Run only on CPU 1
	g_curr_cpuid = 1;
	g_thrd_init = 0;

	g_taskwait = 0;

	if(alloc_thrd_resources()) {
		 printk(KERN_ALERT "%s:%d failed \n",
			 __FUNCTION__, __LINE__);
		return -1;	
	}
	g_thrd_init = 1;
	run_thrds();

	spin_lock_init(&thrdlock);

#endif
   return 0;	
}



int perform_compress(int fd, nvme_cmdrw_t *cmdrw, unsigned int fno){

	struct file *fp = NULL;
	size_t isize = 0;
        //void *input = NULL;
	char *outbuf = NULL;
	const char __user *compressd = NULL;
	const char __user *p = (void *)cmdrw->common.prp2;
	ssize_t rdbytes=0;
	loff_t fpos = 0; /*Always read from byte 0 */
	size_t compress_len;
	int err = 0;
	int newfd = -1;
	struct crfss_fstruct *newrd = NULL;
	u8 opc = nvme_cmd_append;

#ifdef _DEVFS_SNAPPY_THREADS
	struct compress_arg args;
#endif

	fp = fget(fd);
	if(!fp) {
		printk(KERN_ALERT "%s, %d failed to get file pointer \n",
				__FUNCTION__, __LINE__);
		goto err_perf_comprss;
	}

#ifdef _DEVFS_IO_ENABLE

	isize = fp->f_inode->i_size;

	if(!isize){
		printk(KERN_ALERT "%s, %d file size %lu\n",
                 __FUNCTION__, __LINE__, isize);
		goto err_perf_comprss;
	}

#ifdef _DEBUG
	crfss_dbgv( "%s:%d File size isize %zu pos %llu\n", 
		__FUNCTION__, __LINE__,isize, fp->f_pos);
#endif

#if 0
	input = kmalloc(isize, GFP_KERNEL);
        if(!input) {
		
		printk(KERN_ALERT "%s, %d vmalloc fail\n",
				 __FUNCTION__, __LINE__);
		goto err_perf_comprss;
	}
#endif
//This is not defined. Remove this ifdef after testing
	rdbytes = crfss_read(fp, (char __user *)p, isize, &fpos);
#else
	isize = 4096;
	rdbytes = isize;
#endif
	/*oldrd = fd_to_queuebuf(fd);
	cmdrw->nlb = compress_len;
	rdbytes = vfio_crfss_io_read (oldrd, cmdrw, 1);*/
	if(rdbytes != isize) {
		printk(KERN_ALERT "%s, %d crfss_read fail read %zu memloc %lu\n",
				 __FUNCTION__, __LINE__, rdbytes, (unsigned long)p);
		goto err_perf_comprss;
	}
#ifdef _DEBUG
	crfss_dbgv( "%s:%d File read bytes %zu memloc %lu \n", 
		__FUNCTION__, __LINE__,rdbytes, (unsigned long)p);
#endif
	outbuf = kmalloc(isize, GFP_USER);
	//compressd = (void *)p + isize;

        if(!outbuf) {
		printk(KERN_ALERT "%s, %d vmalloc fail\n",
				 __FUNCTION__, __LINE__);
		goto err_perf_comprss;
	}
	//memset(compressd, 0, isize);
	/* Force it to a user buffer */
	compressd = (__force char __user *)outbuf; 

#ifdef _DEVFS_SNAPPY_THREADS
	spin_lock(&thrdlock);
	//smp_mb();
	g_compress_arg.p = p;
	g_compress_arg.isize = isize;
	g_compress_arg.compressd = outbuf;
	compress_len = 0;
	g_compress_arg.compress_len = &compress_len;
	g_compress_arg.task_pending = 1;
	smp_mb();
	spin_unlock(&thrdlock);
	//compress_thread_func(&args);
	//threads[0] = kthread_run(compress_thread_func, &g_compress_arg, "compress thread");
	while (!compress_len) {
		//smp_mb();
		cpu_relax();
	}
	//compress_len = *(g_compress_arg.compress_len);
	crfss_dbgv("g_compressd addr %lu compress_len %u\n", 
			g_compress_arg.compressd, compress_len);
#else
	if((err = snappy_compress(&g_env, p, isize, compressd, &compress_len)) != 0) {
		printk(KERN_ALERT "%s, %d compress failed\n",
					 __FUNCTION__, __LINE__);
	}
#endif
#ifdef _DEBUG
	crfss_dbgv( "%s:%d Compression success %zu\n",
			 __FUNCTION__, __LINE__, compress_len);
#endif

//Remove this ifdef for proper functioning of code
#ifdef _DEVFS_IO_ENABLE
	/* create a new output file */
	newfd = create_file(fno, "output");
	if(newfd < 0) {
        	 printk(KERN_ALERT "%s:%d create_file create failed %d\n",
                          __FUNCTION__, __LINE__, newfd);
	         goto err_perf_comprss;
	}
	newrd = fd_to_queuebuf(newfd);
	if (!newrd) {
	 	 printk(KERN_ALERT "%s:%d fd_to_queuebuf failed %d\n",
                          __FUNCTION__, __LINE__, newfd);
	         goto err_perf_comprss;
	}
	cmdrw->nlb = compress_len;
	cmdrw->common.prp2 = (u64)compressd;
	rdbytes = perform_write(newfd, cmdrw, opc, newrd);

	if(rdbytes != compress_len) {
	 	 printk(KERN_ALERT "%s:%d perform_write failed %lu\n",
                          __FUNCTION__, __LINE__, rdbytes);
	         goto err_perf_comprss;
	}
	//Set the pointer to input buffer back.
	cmdrw->common.prp2 = (u64)p;
	//close the file
	if (crfss_close(fp->f_inode, fp)) {

	 	 printk(KERN_ALERT "%s:%d crfss_close failed\n",
                          __FUNCTION__, __LINE__);
	         goto err_perf_comprss;
	}
#endif

err_perf_comprss:
#if 0
	if(input) 
		kfree(input);
#endif
	if(outbuf) 
		kfree(outbuf);

	if (err) 
		printk(KERN_ALERT "%s:%d Snappy failed \n", 
					__FUNCTION__, __LINE__);
	return -1;
}


int crfss_snappy_test(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw,
		size_t sz, int fd, int append){

	unsigned int numfiles = (unsigned int)cmdrw->common.prp1; 
	unsigned int fidx = 0;
	nvme_cmdrw_t *cmd = (nvme_cmdrw_t *)cmdrw;
	long retval = 0;
	u8 opc;
	//unsigned int numblocks = 4;
	opc = nvme_cmd_append;
	int cpuidx = g_curr_cpuid;

	char *input = kmalloc(8192, GFP_USER);
	memset(input, 'b', 8192);
	cmdrw->common.prp2 = (u64)input;


	struct timeval start;
	struct timeval stop;
	s64 elapsedns;

#ifdef _DEVFS_SNAPPY_THREADS
	unsigned int *t_fd;
	unsigned int *t_fno;
	nvme_cmdrw_t *t_cmdrw;
	u8 *t_pending;
	u8 *t_finish;
#endif

        if(!g_snappy_init) {
		
		if(test_snappy_init_env()) 
			goto err_crfss_snappy_test;			

                g_snappy_init = 1;

                crfss_dbgv("%s:%d test_snappy_init_env  success\n",
                 __FUNCTION__, __LINE__);
        }

	/* If the number of files is 0, switch to default */
	if(!numfiles) {
		numfiles = DEVFS_SNAPPY_FILES;
		printk(KERN_ALERT "%s:%d numfiles not set, using default \n", numfiles);
	}
	
	crfss_dbgv( "Starting %s:%d for %d blocks and  "
			"for %d files with base fd %d\n", __FUNCTION__,__LINE__, 
			DEVFS_BLOCKS, numfiles, fd);

	do_gettimeofday(&start);

	for (fidx=0; fidx < numfiles; fidx++) {	

	        fd = create_file(fidx+1, "input");
		 if(fd < 0) {
                 	printk(KERN_ALERT "%s:%d create_file create failed %d\n",
                          __FUNCTION__, __LINE__, fd);
	                 goto err_crfss_snappy_test;
        	 }
#ifdef _DEBUG
                crfss_dbgv( "%s:%d create_file returns %d\n",
                          __FUNCTION__, __LINE__, fd);
#endif
		rd =  fd_to_queuebuf(fd);
		if(!rd) {
                 	printk(KERN_ALERT "%s:%d fd_to_queuebuf failed %d\n",
                          __FUNCTION__, __LINE__, fd);
	                 goto err_crfss_snappy_test;
		}

#if 0 //_DEVFS_SNAPPY_THREADS

		if(!g_thrd_init) {
			BUG_ON(!g_thrd_init);
		}

		taskargs[cpuidx].fd = fd;
		taskargs[cpuidx].cmdrw = cmd;  
		taskargs[cpuidx].fno = numfiles+fidx;
		taskargs[cpuidx].task_pending = 1;
		taskargs[cpuidx].task_finish = 0;
		taskargs[cpuidx].cpuid = cpuidx;

		 printk(KERN_ALERT "%s:%d, PARENT cpuid %d task_pending %d, "
			         "task_finish %d cmdrw %lu \n", __FUNCTION__, __LINE__,
			         cpuidx, taskargs[cpuidx].task_pending,
			         taskargs[cpuidx].task_finish, taskargs[cpuidx].cmdrw->common.prp2);

		/*crfss_flush_buffer(taskargs[cpuidx], 
				sizeof(struct task_arg), 1);*/
		//threads[cpuidx] = kthread_run(compress_func, &taskargs[cpuidx], "compress thread");
		compress_func(&taskargs[cpuidx]);
		//wake_up_process(threads[cpuidx]);
		printk(KERN_ALERT "%s:%d Dispatched task, waiting \n", __FUNCTION__, __LINE__);
		while (taskargs[cpuidx].task_pending && !taskargs[cpuidx].task_finish);
		taskargs[cpuidx].task_pending = 0;
		taskargs[cpuidx].task_finish = 1;
		printk(KERN_ALERT "%s:%d Dispatched task,  finished \n", __FUNCTION__, __LINE__);
#else
		/* numfiles is used to generate an output file name 
		that does not conflict with the input file */
		perform_compress(fd, cmd, numfiles+fidx);
#endif
	}

#ifdef _DEVFS_SNAPPY_THREADS
	free_thrd_resources(NUM_CPUS);
#endif
	do_gettimeofday(&stop);
	elapsedns = timeval_to_ns(&stop) - timeval_to_ns(&start);
	printk(KERN_INFO "Elapsed time:%llu ns, %f  sec ",  
			elapsedns, (float)elapsedns/(float)1000000);

//finished:
err_crfss_snappy_test:
	printk(KERN_ALERT "Completed Snappy test for %u files with %d  "
		"blocks on each file\n", fidx,  DEVFS_BLOCKS);
	return retval;
}
#endif


int crfss_rwtest(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw,
		size_t sz, int fd, int append){

	unsigned int i = 0;
	nvme_cmdrw_t *cmd = (nvme_cmdrw_t *)cmdrw;
	long retval = 0;
	unsigned long maxio = 16*1024*1024*1024L;
	u8 opc;
	struct file *fp = NULL;

#if defined(_DEVFS_SNAPPY_TEST)
	unsigned long numblocks = 4;
#else
	//unsigned long numblocks = 25000;
	unsigned long numblocks = 1;
#endif
	opc = cmdrw->common.opc;

#if defined(_DEVFS_CONCURR_TEST)
	//return crfss_run_test(cmd);
#endif

	unsigned int numfiles = (unsigned int)cmdrw->common.prp1;
	unsigned int idx = 0;

        for (idx=0; idx <= numfiles; idx++) {

                struct crfss_fstruct *newrd = NULL;
                fd = create_file((current->pid*10) + idx, "tid");
                newrd = fd_to_queuebuf(fd);
                if (!newrd) {
                         printk(KERN_ALERT "%s:%d fd_to_queuebuf failed %d\n",
                          __FUNCTION__, __LINE__, fd);
                         continue;
                }

                fp = fget(fd);
                if(!fp) {
                        printk(KERN_ALERT "%s, %d failed to get file pointer \n",
                                        __FUNCTION__, __LINE__);
                        continue;
                }
		rd = newrd;

#if 0
		printk(KERN_ALERT "Starting %s:%d for %lu blocks\n",
				__FUNCTION__,__LINE__, numblocks);
#endif

//#if defined(ENABLE_KERNTEST_RW)
		for (i=0; i <= 1; i++) {

			cmd->nlb = 4096;

			if ((i * cmd->nlb) > maxio) {
				goto finished;
			}
			if ((opc == nvme_cmd_write) || (opc == nvme_cmd_append)) {
	#if defined(_DEVFS_DIRECTIO)
				retval =  crfss_direct_write (rd,(void *)cmd, append);
	#else
				retval = rd_write(rd, (void *)cmd, sizeof(nvme_cmdrw_t), fd, append);
	#endif
			}
			else if(opc == nvme_cmd_read) {
				retval = vfio_crfss_io_read (rd, cmdrw, 1);
			}
		}
//#endif
		//close the file
		if (crfss_close(fp->f_inode, fp)) {

			 printk(KERN_ALERT "%s:%d crfss_close failed\n",
				  __FUNCTION__, __LINE__);
			 goto finished;
		}

	}

#if defined(_DEVFS_SNAPPY_TEST)
	crfss_snappy_test(rd, cmdrw, sz, fd, append);
#endif

finished:
	printk(KERN_ALERT "Completed %lu blocks nlb %llu \n", numblocks,  cmd->nlb);
	return retval;
}






int crfss_thread_append(void *data){

	volatile struct task_arg *arg = (struct task_arg *)data;
	nvme_cmdrw_t *cmdrw = arg->cmdrw;
	int fd = -1;
	unsigned int i = 0;
	u8 opc;
	unsigned long numblocks = 25000;
	opc = cmdrw->common.opc;
	size_t rdbytes = 0;
	unsigned int numfiles = (unsigned int)cmdrw->common.prp1;
	int append = 1;
	struct file *fp = NULL;

	printk(KERN_ALERT "Starting %s:%d for %u numfiles %lu blocks\n",
			__FUNCTION__,__LINE__, numfiles, numblocks);

	/*cmdrw->nlb = PAGE_SIZE;
	const char __user *outbuf = NULL;
	outbuf = kmalloc(cmdrw->nlb, GFP_USER);
        if(!outbuf) {
                printk(KERN_ALERT "%s:%d kmalloc fail\n",
                                 __FUNCTION__, __LINE__);
		goto finished;
        }
        memset(outbuf, 'a', cmdrw->nlb);
        cmdrw->common.prp2 = (__force char __user *)outbuf;*/

	for (i=0; i <= numfiles; i++) {

		struct crfss_fstruct *newrd = NULL;
		fd = create_file(i, "tid");
	        newrd = fd_to_queuebuf(fd);
	        if (!newrd) {
        	         printk(KERN_ALERT "%s:%d fd_to_queuebuf failed %d\n",
                          __FUNCTION__, __LINE__, fd);
			 continue;
        	}

		fp = fget(fd);
		if(!fp) {
			printk(KERN_ALERT "%s, %d failed to get file pointer \n",
					__FUNCTION__, __LINE__);
			continue;
		}

		for (i=0; i <= numblocks; i++) {
#if 1
			if ((opc == nvme_cmd_write) || (opc == nvme_cmd_append)) {
				rdbytes = perform_write(fd, cmdrw, opc, newrd);
				if(rdbytes != cmdrw->nlb) {
					 printk(KERN_ALERT "%s:%d perform_write failed %lu\n",
						  __FUNCTION__, __LINE__, rdbytes);
				}
			}
#else
			if ((opc == nvme_cmd_write) || (opc == nvme_cmd_append))
	                        rdbytes =  crfss_direct_write (newrd,(void *)cmdrw, append);

			//Read not supported yet
			/*else if(opc == nvme_cmd_read) {
				rdbytes = vfio_crfss_io_read (rd, cmdrw, 1);
			}*/
#endif
		}

		//close the file
		if (crfss_close(fp->f_inode, fp)) {

			 printk(KERN_ALERT "%s:%d crfss_close failed\n",
				  __FUNCTION__, __LINE__);
			  continue;
		}
	}
finished:
	//complete(arg->comp);
	printk(KERN_ALERT "Completed %lu blocks nlb %llu \n", numblocks, cmdrw->nlb);
	return rdbytes;
}


int crfss_run_test(nvme_cmdrw_t *cmdrw){

	volatile struct task_arg task;
	struct task_struct *rwthrd;

	task.cmdrw = cmdrw;

	//rwthrd = kthread_run(crfss_thread_append, &task, "rwthread");
	crfss_thread_append(&task);
#if 0
	if (!rwthrd) {
		printk(KERN_ALERT "%s:%d kthread_run failed \n",
			__FUNCTION__, __LINE__);
		goto error_runtest;
	}
	wait_for_completion(&comp); 
#endif
error_runtest:
	return -1;
}

