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
#include <linux/random.h>
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
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/namei.h>
#include <linux/fdtable.h>
#include <linux/interval_tree.h>
#include "pmfs.h"
#include "xip.h"

#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>
#include <xen/xen.h>
#include <xen/interface/xen.h>
#include <xen/interface/memory.h>
#include <xen/balloon.h>
#include <xen/heteromem.h>
#include <xen/features.h>
#include <xen/page.h>


//#define DRIVER_VERSION  "0.2"
//#define DRIVER_AUTHOR   "Sudarsun Kannan <sudarsun.kannan@gmail.com>"
//#define DRIVER_DESC     "DevFS filesystem"

#define DRIVER_VERSION  "0.2"
#define DRIVER_AUTHOR   "Alex Williamson <alex.williamson@redhat.com>"
#define DRIVER_DESC     "Type1 IOMMU driver for VFIO"

/*################## DEVFS ##################################*/

DEFINE_MUTEX(temp_mutex);

extern struct file_system_type crfss_fs_type;

static int g_crfss_init;
static struct crfss_inotree *g_inotree;
static __u8 iskernelio;
static __u32 g_qentrycnt;

#ifdef CRFS_MULTI_PROC
int g_crfss_scheduler_init[HOST_PROCESS_MAX] = {0};
#else
int g_crfss_scheduler_init = 0;
#endif

#ifdef CRFS_OPENCLOSE_OPT
#define FD_QUEUE_POOL_PG_NUM 512
static __u64 dma_able_user_addr;
static __u64 dma_able_fd_queue_map[FD_QUEUE_POOL_PG_NUM];
#endif

static int vfio_creatfs_cmd(unsigned long arg);
static int vfio_creatq_cmd(unsigned long arg);

static int crfss_init_file_queue(struct crfss_fstruct *rd){

#ifndef CRFS_BYPASS_KERNEL
	rd->fifo.buf = kzalloc(QUEUESZ(rd->qentrycnt), GFP_KERNEL);
	if (!rd->fifo.buf) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		goto err_init_fqueue;
	}
#endif
	rd->fifo.head = 0;
	rd->fifo.tail = 0;

	rd->qnumblocks = rd->qentrycnt;

	/*set queue buffer initialize to true*/
	rd->fqinit = 1;

	return 0;

#ifndef CRFS_BYPASS_KERNEL
err_init_fqueue:
	return -1;
#endif
}


/* DEVFS initialize function to initialize queue.
 * Command buffer is also initialized */
struct crfss_fstruct *crfss_init_file_struct(unsigned int qentrycnt){

	struct crfss_fstruct *rd =
			kzalloc(sizeof(struct crfss_fstruct), GFP_KERNEL);
	if (!rd) {
		printk(KERN_ALERT "Alloc failed %s:%d \n",__FUNCTION__,__LINE__);
		goto err_crfss_init;
	}

	if (qentrycnt)
		rd->qentrycnt = qentrycnt;
	else
		rd->qentrycnt = 32;

#if defined(_DEVFS_DEBUG_RDWR)
	crfss_dbg("DEBUG: QSIZE %lu entries %u\n",
			QUEUESZ(rd->qentrycnt), rd->qentrycnt);
#endif

	if (rd->init_flg){
		goto err_crfss_init;
	}

	crfss_mutex_init(&rd->read_lock);
	init_waitqueue_head(&rd->fifo_event);

	rd->fqinit = 0;
	rd->num_entries = 0;
	rd->queuesize = QUEUESZ(rd->qentrycnt);
	rd->entrysize = sizeof(__u64);

	rd->init_flg = 1;

	/* If Lazy alloc is defined, we allocate only during first write*/
#if !defined(_DEVFS_ONDEMAND_QUEUE)
	if (crfss_init_file_queue(rd))
		goto err_crfss_init;    	
#endif

#if defined(_DEVFS_DEBUG_RDWR)
	crfss_dbg("DEBUG: %s:%d fs queue buff num blocks %llu \n",
			__FUNCTION__,__LINE__, (__u64) rd->qnumblocks);
#endif
	return rd;

err_crfss_init:
	return NULL;
}


int crfss_free_file_queue(struct crfss_fstruct *rd) {
#if defined(_DEVFS_MEMGMT)
	struct page *page = NULL;
#endif

	if (!rd->fqinit) {
		/* No writes to file, and using ondemand allocation, 
		so skip deallocation */
		goto skip_qbuf_free;
	}

#ifndef CRFS_OPENCLOSE_OPT
	if (!rd->qnumblocks || rd->fifo.buf == NULL)
		return -1;
#endif

/*#ifdef _DEVFS_KTHREAD_ENABLED
	crfss_scalability_flush_buffer(rd);
#else
	vfio_crfss_io_write (rd);
#endif*/

#ifndef CRFS_BYPASS_KERNEL
	if (rd->fifo.buf) {
		kfree(rd->fifo.buf);
		rd->fifo.buf = NULL;
	}
	rd->fifo.head = rd->fifo.tail = 0;
#endif

skip_qbuf_free:
	rd->fsblocks = NULL;
	rd->qnumblocks = 0;
	rd->num_entries = 0;
	rd->init_flg = 0;
	rd->fqinit = 0;

#if defined(_DEVFS_DEBUG)
	printk("DEBUG: Finished driver cleanup %s:%d \n",
			__FUNCTION__, __LINE__);
#endif
	return 0;
}	


/*
 * Responsible for writing to the kernel submission queue buffer
 * by copying data from the user-level buffer.
 * When the queue is full, IO is performed to the opened file
 */
int rd_write(struct crfss_fstruct *rd, void *buf, 
		int sz, int fd, int append)
{
	/* This function is deprecated*/
	return 0;
}


/* Direct write or append - Directly writes from the user buffer to DevFS file
 */
long crfss_direct_write(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw, u8 isappend)
{
	/* This function is deprecated*/
	return 0;
}


#ifndef CRFS_BYPASS_KERNEL
/*
 * Dequeue per-file pointer queue
 */
nvme_cmdrw_t *rd_dequeue(struct crfss_fstruct *rd, int sz)
{
	struct circ_buf *fifo = &rd->fifo;
	nvme_cmdrw_t *cmd = NULL;
	//unsigned long head = smp_load_acquire(&fifo->head);
	unsigned long head = fifo->head;
	unsigned long tail = fifo->tail;


	if (head != tail) {
		cmd = (nvme_cmdrw_t*)(*((__u64*)(fifo->buf + tail)));
	}

	//smp_store_release(&fifo->tail, (tail + rd->entrysize) & (rd->queuesize - 1));
	fifo->tail = (fifo->tail + rd->entrysize) & (rd->queuesize - 1);

	return cmd;
}

/*
 * Read the tail of per-file pointer queue
 */
nvme_cmdrw_t *rd_queue_readtail(struct crfss_fstruct *rd, int sz)
{
	struct circ_buf *fifo = &rd->fifo;
	nvme_cmdrw_t *cmd = NULL;
	//unsigned long head = smp_load_acquire(&fifo->head);
	unsigned long head = fifo->head;
	unsigned long tail = fifo->tail;

	if (head != tail) {
		cmd = (nvme_cmdrw_t*)(*((__u64*)(fifo->buf + tail)));
	}

	return cmd;
}

/*
 * Enqueue per-file pointer queue
 */
int rd_enqueue(struct crfss_fstruct *rd, int sz, nvme_cmdrw_t *cmd)
{
	struct circ_buf *fifo = &rd->fifo;
	unsigned long head = fifo->head;
	//unsigned long tail = READ_ONCE(fifo->tail);
	unsigned long tail = fifo->tail;

	memcpy(fifo->buf + head, &cmd, sizeof(__u64));

	//smp_store_release(&fifo->head, (head + rd->entrysize) & (rd->queuesize - 1));
	fifo->head = (fifo->head + rd->entrysize) & (rd->queuesize - 1);

	return 0;
}
#else
/*
 * Dequeue per-file pointer queue
 */
nvme_cmdrw_t *rd_dequeue(struct crfss_fstruct *rd, int sz)
{
	struct circ_buf *fifo = &rd->fifo;
	nvme_cmdrw_t *cmd = NULL;
	unsigned long tail = fifo->tail;

	cmd = (nvme_cmdrw_t*)(fifo->buf + tail);

	/* Currrently the queue size is page size */
	fifo->tail = (fifo->tail + sizeof(nvme_cmdrw_t)) & (PAGE_SIZE - 1);

	return cmd;
}

/*
 * Read the tail of per-file pointer queue
 */
nvme_cmdrw_t *rd_queue_readtail(struct crfss_fstruct *rd, int sz)
{
	struct circ_buf *fifo = &rd->fifo;
	nvme_cmdrw_t *cmd = NULL;
	unsigned long tail = fifo->tail;

	cmd = (nvme_cmdrw_t*)(fifo->buf + tail);

	if (cmd && cmd->status == (DEVFS_CMD_READY | DEVFS_CMD_BUSY))
		return cmd;
	else
		return NULL;
}

/*
 * Enqueue per-file pointer queue
 */
int rd_enqueue(struct crfss_fstruct *rd, int sz, nvme_cmdrw_t *cmd)
{
	/* For no-ioctl code path, enqueue is done in Libfs */
	return 0;
}
#endif

/*
 * Read handler
 */
long vfio_crfss_io_read (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdreq, 
			u8 isappend){

	int num_entries = 0;
	long ret = 0;
	ssize_t rdbytes=0;
	struct file *fp = rd->fp;
	loff_t fpos = rd->fpos;	
	void *buf = (void *)cmdreq->common.prp2;
	ssize_t reqsz = cmdreq->nlb;
	char *p = NULL;

#ifndef CRFS_PERF_TUNING
	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;

	inode = fp->f_inode;
	if (!inode) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto io_read_err;
	}

	ei = DEVFS_I(inode);
	if (!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto io_read_err;
	}
#endif

#ifndef CRFS_SCHED_THREAD
	if (!cmdreq->blk_addr) {
	       printk(KERN_ALERT "%s:%d Read buffer is not allocated \n",
				__FUNCTION__, __LINE__); 
	       ret = -EFAULT;
               goto io_read_err;
	}
        p = (__force char __user *)cmdreq->blk_addr; 
#else
	p = (char *)cmdreq->common.prp2;
#endif
	num_entries = rd->num_entries;

	/* check credential here */
	if(crfss_check_fs_cred(rd)) {
		printk(KERN_ALERT "%s:%d Read perm failed \n",__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto io_read_err;
	}

	if (crfss_check_cred_table(cmdreq->cred_id)) {
		printk(KERN_ALERT "%s:%d Cred check failed \n",__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto io_read_err;
	}


	/*TODO: Currently, we only read in page size. should be more than that */
#ifndef CRFS_PERF_TUNING
	if (fp != NULL && buf) 
#endif
	{
		if (isappend) {
			/* Sequential Read, just follow current pos of this file pointer */
			fpos = (loff_t)fp->f_pos;
		} else {
			/* Random Read, just use the offset specified from user space */
			fpos = (loff_t)cmdreq->slba;
		}

#ifndef _DEVFS_XIP_IO
		rdbytes = crfss_read(fp, p, reqsz, &fpos);
#else
		rdbytes = crfs_xip_file_read(fp, p, reqsz, &fpos);
#endif
		if (!rdbytes) {
			//printk(KERN_ALERT "read failed, rdbytes = %d, inode = %llx, "
			//	"isize = %lu, count = %lu, pos = %llu\n",
			//	rdbytes, fp->f_inode, fp->f_inode->i_size, reqsz, cmdreq->slba);
			ret = (long)rdbytes;
		} 
		ret = (long)rdbytes;	
	}

	if (isappend)
		fp->f_pos = fp->f_pos + rdbytes;

	/* Remove this request from fp queue when I/O is done */
	rd_dequeue(rd, sizeof(__u64));

io_read_err:
	return ret;
}

/*
 * Append handler
 */

/* Debug: Read all the current buffer queue values 
 * The reader function holds the read lock buffer reading each 
 * read buffer values 
 */
int vfio_crfss_io_append (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {

	int ret = 0;
	ssize_t wrbytes=0;
	struct file *fp = rd->fp;
	loff_t fpos = rd->fpos;	
	void *src = NULL;
	const char __user *p;

#ifndef CRFS_PERF_TUNING
	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;
	int num_entries = 0;

	num_entries = rd->num_entries;

	inode = fp->f_inode;
	if (!inode) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto appendio_err;
	}

	ei = DEVFS_I(inode);
	if (!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto appendio_err;
	}

	if (!cmdrw) {
		printk(KERN_ALERT "FAILED %s:%d cmdrw is null \n",
				__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto appendio_err;
	}
#endif
	/* check credential here */
	if (crfss_check_cred_table(cmdrw->cred_id)) {
		printk(KERN_ALERT "%s:%d Cred check failed \n",__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto appendio_err;
	}

	/* Now both ioctl and no-ioctl are async write */
	src = (void *)cmdrw->blk_addr;

	wrbytes = 0;	
	cmdrw->slba = fp->f_pos;


	fpos = (loff_t)cmdrw->slba;
	p = (__force const char __user *)src;

#ifndef _DEVFS_XIP_IO
	wrbytes = crfss_kernel_write(fp, p, cmdrw->nlb, &fpos, cmdrw->blk_addr);
#else
	wrbytes = crfs_xip_file_write(fp, p, cmdrw->nlb, &fpos);
#endif

	if (!wrbytes || (wrbytes != cmdrw->nlb)) {
		ret = -EFAULT;
		printk("%s:%u Write failed fd %d slba %llu off %llu size %zu\n",
				__FUNCTION__, __LINE__, rd->fd, (loff_t)cmdrw->slba, fpos, wrbytes);
		goto appendio_err;
	}

	fp->f_pos = fp->f_pos + wrbytes;
	ret = wrbytes;

#ifndef CRFS_BYPASS_KERNEL
	/* Remove entry in per-fp submission tree */	
	crfss_write_submission_tree_delete(inode, cmdrw);
	/* Release kernel data buffer */
	put_data_buffer(rd, cmdrw);

	cmdrw->blk_addr = NULL;
#endif

	/* Remove current request from queue when finish */
	rd_dequeue(rd, sizeof(__u64));

appendio_err:
	return ret;
}

/*
 * Write handler
 */

/* Debug: Read all the current buffer queue values 
 * The reader function holds the read lock buffer reading each 
 * read buffer values 
 */
int vfio_crfss_io_write (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {

	int num_entries = 0, i = 0;
	int ret = 0;
	ssize_t wrbytes=0;
	struct file *fp = rd->fp;
	loff_t fpos = rd->fpos;	

	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;

	void *src = NULL;
	const char __user *p;

	num_entries = rd->num_entries;

	inode = fp->f_inode;
	if (!inode) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto writeio_err;
	}

	ei = DEVFS_I(inode);
	if (!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto writeio_err;
	}

	if (!cmdrw) {
		printk(KERN_ALERT "FAILED %s:%d cmdrw is null \n",
				__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto writeio_err;
	}

	/* check credential here */
	if (crfss_check_cred_table(cmdrw->cred_id)) {
		printk(KERN_ALERT "%s:%d Cred check failed \n",__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto writeio_err;
	}

	/* Now both ioctl and no-ioctl are async write */
	src = (void *)cmdrw->blk_addr;
	wrbytes = 0;	

	if (!src) {
		printk(KERN_ALERT "FAILED %s:%d block %llu buff idx %d opcode %d null \n",
				__FUNCTION__, __LINE__, cmdrw->slba, i, cmdrw->common.opc);
		ret = -EFAULT;
		goto writeio_err;
	}

#if defined(_DEVFS_DEBUG_RDWR)
	DEBUGCMD(cmdrw);
#endif
	if (fp != NULL && src) {
		fpos = (loff_t)cmdrw->slba;
		p = (__force const char __user *)src;

#ifndef _DEVFS_XIP_IO
		wrbytes = crfss_kernel_write(fp, p, cmdrw->nlb, &fpos, cmdrw->blk_addr);
#else
		wrbytes = crfs_xip_file_write(fp, p, cmdrw->nlb, &fpos);
#endif

		if (!wrbytes || (wrbytes != cmdrw->nlb)) {
			ret = -EFAULT;

			printk("%s:%u Write failed fd %d slba %llu off %llu size %zu\n",
					__FUNCTION__, __LINE__, rd->fd, (loff_t)cmdrw->slba, fpos, wrbytes);

			goto writeio_err;
		}
	}

	ret = wrbytes;

#ifndef CRFS_BYPASS_KERNEL
	/* Remove entry in per-fp submission tree */
	crfss_write_submission_tree_delete(inode, cmdrw);

	/* Release kernel data buffer */
	put_data_buffer(rd, cmdrw);

	cmdrw->blk_addr = NULL;
#endif

	/* Remove current request from queue when finish */ 
	rd_dequeue(rd, sizeof(__u64));

writeio_err:
	return ret;
}

#ifdef _DEVFS_KTHREAD_DISABLED
long vfio_crfss_io_read_host_thread (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdreq, 
			u8 isappend){

	int num_entries = 0;
	long ret = 0;
	ssize_t rdbytes=0;
	struct file *fp = rd->fp;
	loff_t fpos = rd->fpos;	
	void *buf = (void *)cmdreq->common.prp2;
	ssize_t reqsz = cmdreq->nlb;
	int i = 0, j = 0;
	nvme_cmdrw_t *cmdrw = NULL;
	
	void *src = NULL;
	loff_t index = 0;

	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;

	char *p = (__force char __user *)cmdreq->common.prp2; 

	num_entries = rd->num_entries;

	inode = fp->f_inode;
	if(!inode) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto io_read_err;
	}

	ei = DEVFS_I(inode);
	if(!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto io_read_err;
	}

	if(!cmdreq) {
		printk(KERN_ALERT "%s, %d request null \n", __FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto io_read_err;
	}

	/*check credential here */
	if(crfss_check_fs_cred(rd)) {
		printk(KERN_ALERT "%s:%d Read perm failed \n",__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto io_read_err;
	}


search_disk:

	/*TODO: Currently, we only read in page size. should be more than that */
	if (fp != NULL && buf) {
		if (isappend) {
			// Sequential Read, just follow current pos of this file pointer
			fpos = (loff_t)fp->f_pos;
		} else {
			// Random Read, just use the offset specified from user space
			fpos = (loff_t)cmdreq->slba;
		}

#ifndef _DEVFS_XIP_IO
		rdbytes = crfss_read(fp, p, reqsz, &fpos);
#else
		rdbytes = crfs_xip_file_read(fp, p, reqsz, &fpos);
#endif
		if (!rdbytes) {
			printk(KERN_ALERT "read failed, rdbytes = %d, inode = %llx, "
				"isize = %lu, count = %lu, pos = %llu\n",
				rdbytes, fp->f_inode, fp->f_inode->i_size, reqsz, cmdreq->slba);
			ret = (long)rdbytes;
		} 
		ret = (long)rdbytes;	

	}

	//printk(KERN_ALERT "rdbytes = %d\n", rdbytes);

	if (isappend)
		fp->f_pos = fp->f_pos + rdbytes;

ioread_finish:
	return ret;

io_read_err:
	return ret;
}


/* Debug: Read all the current buffer queue values 
 * The reader function holds the read lock buffer reading each 
 * read buffer values 
 */
int vfio_crfss_io_append_host_thread (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {

	void *ptr;
	int num_entries = 0, i = 0;
	int ret = 0;
	ssize_t wrbytes=0;
	struct file *fp = rd->fp;
	loff_t fpos = rd->fpos;	
	loff_t index = 0;

	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;

	void *src = NULL;
	const char __user *p;

	num_entries = rd->num_entries;

	inode = fp->f_inode;
	if (!inode) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto appendio_err;
	}

	ei = DEVFS_I(inode);
	if (!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		ret = -EFAULT;
		goto appendio_err;
	}

	if (!cmdrw) {
		printk(KERN_ALERT "FAILED %s:%d cmdrw is null \n",
				__FUNCTION__, __LINE__);
		ret = -EFAULT;
		goto appendio_err;
	}

	src = (void *)cmdrw->common.prp2;
	wrbytes = 0;	
	cmdrw->slba = fp->f_pos;

	if (!src) {
		printk(KERN_ALERT "FAILED %s:%d block %llu buff idx %d opcode %d null \n",
				__FUNCTION__, __LINE__, cmdrw->slba, i, cmdrw->common.opc);
		ret = -EFAULT;
		goto appendio_err;
	}

#if defined(_DEVFS_DEBUG_RDWR)
	DEBUGCMD(cmdrw);
#endif
	if (fp != NULL && src) {

		fpos = (loff_t)cmdrw->slba;
		p = (__force const char __user *)src;

#ifndef _DEVFS_XIP_IO
		wrbytes = crfss_kernel_write(fp, p, cmdrw->nlb, &fpos, cmdrw->blk_addr);
#else
		wrbytes = crfs_xip_file_write(fp, p, cmdrw->nlb, &fpos);
#endif

		if (!wrbytes || (wrbytes != cmdrw->nlb)) {
			ret = -EFAULT;

			printk("%s:%u Write failed fd %d slba %llu off %llu size %zu\n",
					__FUNCTION__, __LINE__, rd->fd, (loff_t)cmdrw->slba, fpos, wrbytes);

			goto appendio_err;
		}
	}
	fp->f_pos = fp->f_pos + wrbytes;
	
	//printk("fp->fpos = %d\n", fp->f_pos);

	ret = wrbytes;

appendio_err:
	return ret;
}
#endif

/*
 * lseek handler
 */
long crfss_llseek(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw)
{

	//BUG TODO: using slba for whence,
	//Ideally, we should be using a flag field in cmdrw flag.
	struct file *file = rd->fp;
	loff_t offset = (loff_t)cmdrw->nlb;
	loff_t currpos = 0;
	int whence = (int)cmdrw->slba;
	long pos = 0;

#if defined(_DEVFS_DEBUG_RDWR)
	printk("%s:%u Before offset %lu, fpos %llu currpos %zu \n",
		__FUNCTION__, __LINE__, offset, file->f_pos, currpos);
#endif

	currpos = default_llseek(file, offset, whence);

	pos = (long)currpos;

#if defined(_DEVFS_DEBUG_RDWR)
	printk("%s:%u Returns offset %lu, fpos %llu currpos %zu pos %ld \n",
		__FUNCTION__, __LINE__, offset, file->f_pos, currpos, pos);
#endif
	
	return pos;
}

/*
 * fsync handler
 */
int vfio_crfss_io_fsync(struct crfss_fstruct *rd) {

	nvme_cmdrw_t *cmdrw;
	int ret = 0;

	/* Flush all requests in this FD-queue until reaching the fsync barrier */ 
flush_writes:
	//printk(KERN_ALERT "fsync ing..., rd = %llx\n", rd);
	cmdrw = rd_queue_readtail(rd, sizeof(__u64));
	if (!cmdrw)
		goto io_fsync_err;

	if (cmdrw->common.opc == nvme_cmd_write) {
		/* Flush write request */
		ret = vfio_crfss_io_write(rd, cmdrw);
		cmdrw->ret = ret;
		test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

		goto flush_writes;
	} else if (cmdrw->common.opc == nvme_cmd_append) {
		/* Flush append request */
		ret = vfio_crfss_io_append(rd, cmdrw);
		cmdrw->ret = ret;
		test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

		goto flush_writes;
	} else if (cmdrw->common.opc == nvme_cmd_read) {
		/* Flush read request */
		if (cmdrw->slba == DEVFS_INVALID_SLBA)
			ret = vfio_crfss_io_read(rd, cmdrw, 1);
		else
			ret = vfio_crfss_io_read(rd, cmdrw, 0);

		cmdrw->ret = ret;
		test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

		goto flush_writes;
	} else if (cmdrw->common.opc == nvme_cmd_flush) {
		/* Reaching the fsync barrier command */
		//printk(KERN_ALERT "Reaching barrier\n");
	}


io_fsync_err:
	//printk(KERN_ALERT "fsync finish, rd = %llx\n", rd);
	return ret;
}
 
/*
 * Wrapper function for processing each I/O request
 *
 * Clear flags after the I/O handler
 */
inline int vfio_process_read(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	int retval = 0;

	if (cmdrw->slba == DEVFS_INVALID_SLBA)
		retval = vfio_crfss_io_read(rd, cmdrw, 1);
	else
		retval = vfio_crfss_io_read(rd, cmdrw, 0);

	/* Update return value of read op */
	cmdrw->ret = retval;

	/* Clear rd request queue header */
	rd->req = NULL;

	/* Clear rd tsc */
	rd->tsc = 0;

	/* Notifying host thread read has finished */
	test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

	return retval;
}

inline int vfio_process_write(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	int retval = 0;

	retval = vfio_crfss_io_write(rd, cmdrw);

	/* Update return value of read op */
	cmdrw->ret = retval;

	/* Clear rd request queue header */
	rd->req = NULL;

	/* Clear rd tsc */
	rd->tsc = 0;

	/* Notifying host thread read has finished */
	test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

	return retval;
}

inline int vfio_process_append(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	int retval = 0;

	retval = vfio_crfss_io_append(rd, cmdrw);

	/* Update return value of read op */
	cmdrw->ret = retval;

	/* Clear rd request queue header */
	rd->req = NULL;

	/* Clear rd tsc */
	rd->tsc = 0;

	/* Notifying host thread read has finished */
	test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

	return retval;
}

inline int vfio_process_fsync(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw,
			struct inode *inode) {
	int retval = 0;

#ifdef _DEVFS_FSYNC_ENABLE
	/*   
	 * when reaching this fsync barrier cmd, all the prior
	 * write/append commands are flushed
	 */

#ifndef CRFS_BYPASS_KERNEL
	/* atomically decrement fsync barrier counter */
	if (atomic_dec_and_test(&inode->fsync_counter) == true)
		clear_bit(0, &inode->fsync_barrier);

	if (atomic_read(&inode->fsync_counter) < 0) 
		printk(KERN_ALERT "Something wrong! %d\n", atomic_read(&inode->fsync_counter));

	rd_dequeue(rd, sizeof(__u64));
#else
	retval = vfio_crfss_io_fsync(rd);

	/* Update return value of fsync op */
	cmdrw->ret = retval;

	/* notify host */
	test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

	rd_dequeue(rd, sizeof(__u64));
             
#endif  // CRFS_BYPASS_KERNEL

#endif	// _DEVFS_FSYNC_ENABLE
	return retval;
}

inline int vfio_process_close(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	int retval = 0;

	retval = vfio_crfss_io_close(rd);

	/* Update return value of read op */
	cmdrw->ret = retval;

	/* Clear rd request queue header */
	rd->req = NULL;

	/* Clear rd tsc */
	rd->tsc = 0;

	/* Notifying host thread read has finished */
	test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

	return retval;
}

inline int vfio_process_unlink(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) {
	int retval = 0;

	retval = vfio_crfss_io_unlink(rd);

	/* Update return value of read op */
	cmdrw->ret = retval;

	/* Clear rd request queue header */
	rd->req = NULL;

	/* Clear rd tsc */
	rd->tsc = 0;

	/* Notifying host thread read has finished */
	test_and_set_bit(0, (volatile long unsigned int *)&cmdrw->status);

	return retval;
}


/*
 * Get rd struct from file descriptor
 */
struct crfss_fstruct *fd_to_queuebuf(int fd) {

	struct crfss_fstruct *rd = NULL;
	struct file *fp = NULL;
	struct fd f = fdget(fd);

	if(fd < 0) {
		printk(KERN_ALERT "%s, %d incorrect file descriptor %d \n",
				__FUNCTION__, __LINE__, fd);
		goto err_queuebuf;
	}

	//fp = fget(fd);
	fp = f.file;
	if(!fp) {
		printk(KERN_ALERT "%s, %d failed to get file pointer \n",
				__FUNCTION__, __LINE__);
		goto err_queuebuf;
	}
	BUG_ON(!fp->isdevfs);
	fdput(f);

	rd = fp->cmdbuf;
	if(!rd) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d for fd %d \n",
				__FUNCTION__,__LINE__, fd);
		goto err_queuebuf;
	}
	return rd;

err_queuebuf:
	return NULL;
}
EXPORT_SYMBOL(fd_to_queuebuf);

/*
 * Get corresponding kernel vaddr of fs library allocated cmd buffer
 * so that the kernel read cmd from buffer directly bypassing ioctl
 */
int map_fd_queue_buffer(struct crfss_fstruct *rd, __u64 vaddr) {
	struct task_struct *task = current;
	struct mm_struct *mm;
	int i = 0, retval = 0;
	struct page *page_info;
	int num_pages = 1;

	pgd_t *pgd;
	pte_t *ptep, pte;
	pud_t *pud;
	pmd_t *pmd;

#ifdef CRFS_OPENCLOSE_OPT
	// Todo
	int idx = (vaddr - dma_able_user_addr) >> 12;

	rd->fifo.buf = (void*)dma_able_fd_queue_map[idx];
	if (!rd->fifo.buf) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
	}
	return retval;
#endif

	mm = task->mm;
	if (!mm) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto ret_map_fd_queue;
	}
	down_read(&mm->mmap_sem);

	for (i = 0; i < num_pages; i++) {

		vaddr = vaddr + (PAGE_SIZE * i);

		pgd = pgd_offset(mm, vaddr);
		if(pgd_none(*pgd) || pgd_bad(*pgd)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			break;
		}
		pud = pud_offset(pgd, vaddr);
		if(pud_none(*pud) || pud_bad(*pud)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			break;
		}
		pmd = pmd_offset(pud, vaddr);
		if(!pmd_none(*pmd) &&
				(pmd_val(*pmd) & (_PAGE_PRESENT|_PAGE_PSE)) != _PAGE_PRESENT) {
			pte = *(pte_t *)pmd;
			//qpfnlist[i] = (pte_pfn(pte) << PAGE_SHIFT) + (vaddr & ~PMD_MASK);
			continue;
		}
		else if (pmd_none(*pmd) || pmd_bad(*pmd)) {
			retval = -EFAULT;
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			break;
		}

		ptep = pte_offset_map(pmd, vaddr);
		if(!ptep) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			break;
		}

		pte = *ptep;
		page_info = pte_page(pte);

		/* 
		 * Get the kernel virtual address from kernel identity map of
		 * the fd queue buffer allocated in user space
		 */
		rd->fifo.buf = page_address(page_info);
		if (!rd->fifo.buf) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
		}

		pte_unmap(ptep);
	}
	up_read(&mm->mmap_sem);

ret_map_fd_queue:
	return retval;
}


/* Create DEVFS file and initalize file in-memory file structures */
//static 
int vfio_creatfile_cmd(unsigned long arg, int kernel_call){

	unsigned long minsz;
	struct vfio_crfss_creatfp_cmd map;
	int retval = 0;
	int flen = 0;
	int flags;
	umode_t mode;
	struct file *fp = NULL;	
	int fd = -1;
	struct crfss_fstruct *rd = NULL;
	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;
	__u8 isjourn = 0;
	unsigned int qentrycnt = 0;
	struct vfio_crfss_creatfp_cmd *mapptr = (struct vfio_crfss_creatfp_cmd *)arg;

#ifdef _DEVFS_SCALABILITY_DBG
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_creatfp_cmd, isrdqueue);
	/*If emulating DevFS I/O calls from within kernel*/
	if (kernel_call) {
		mapptr = (struct vfio_crfss_creatfp_cmd *)arg;
		map = *mapptr;
		if(!strlen(map.fname)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",
				__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_creatfp;
		}	

	} else if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_creatfp;
	}
	flen = strlen(map.fname);
	flags = (int)map.flags;
	mode = (umode_t)map.mode;
	isjourn = (__u8)map.isjourn;
	//isrdqueue = (__u8)map.isrdqueue;
	iskernelio = (__u8)map.iskernelio;
	qentrycnt = (unsigned int)map.qentrycnt;

	if(flen < 0) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d filename "
				"incorrect len %d \n",__FUNCTION__,__LINE__, flen);
		retval = -EFAULT;
		goto err_creatfp;
	}
	crfs_dbg_verbose("%s:%d create file %s isjourn %d "
			"isrdqueue %d iskernelio %d qentrycnt %u\n", 
			__FUNCTION__,__LINE__, map.fname, isjourn, 
			isrdqueue, iskernelio, qentrycnt);

	fp = crfss_create_file(map.fname, flags | O_LARGEFILE, mode, &fd);
	if(fp == NULL) {
		printk(KERN_ALERT "%s:%d create file failed \n", 
				__FUNCTION__,__LINE__);
		retval = fd;
		goto err_creatfp;
	}

	map.fd = fd;
	fp->isdevfs = 1;
	inode = fp->f_inode;
	if(!inode) {
		printk(KERN_ALERT "%s:%d inode NULL \n", 
				__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_creatfp;
	}

	/* initialize devfs specific field in inode */
	inode->isdevfs = 1;

#ifdef _DEVFS_FSYNC_ENABLE
	/* Loop until current fsync is done */
	while (test_bit(0, (volatile long unsigned int *)&inode->fsync_barrier) == 1);
#endif

	/*Get DevFS inode info from inode*/
	ei = DEVFS_I(inode);
	if(!ei) {
		printk(KERN_ALERT "%s:%d DevFS inode info NULL \n", 
				__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_creatfp;
	}

	if(!inode->page_tree_init){
		INIT_RADIX_TREE(&inode->page_tree, GFP_ATOMIC);
		inode->page_tree_init = 1;
	}

	/* allocate file queues and other structures*/
	rd = crfss_init_file_struct(qentrycnt);
	if (!rd) {
		printk(KERN_ALERT "Failed init file struct %s:%d\n",
				__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto err_creatfp;
	}
	fp->cmdbuf = (void *)rd;

	/*Create back reference*/
	rd->fp = fp;
	rd->fd = fd;

	/* Set the task struct pointer as original task */
	rd->task = current;

	/* update security credentials*/
	crfss_set_cred(rd);

#ifdef CRFS_BYPASS_KERNEL
	/*
	 * Register user fs libary allocated buffer
	 * to kernel to provide direct-access without
	 * ioctl()
	 */
	if (map_fd_queue_buffer(rd, map.vaddr)) {
		printk(KERN_ALERT "%s:%d Failed to register fd queue buffer \n",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto err_creatfp;
	}
#endif

#ifndef _DEVFS_KTHREAD_DISABLED
	retval = crfss_scalability_open_setup(ei, rd);

	if (retval != 0) {
		printk(KERN_ALERT "setup fail | %s:%d\n", __FUNCTION__, __LINE__);
		goto err_creatfp;
	}

	if(inode->sq_tree_init != _DEVFS_INITIALIZE){
		printk(KERN_ALERT "%s:%d Radix tree NULL \n",__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto err_creatfp;
	}
#endif

	if(isjourn) {
		ei->isjourn = isjourn;
		/* This condition should be false*/
		/*if(unlikely(ei->cachep_init == CACHEP_INIT)){
			printk(KERN_ALERT "%s:%d Failed \n",
					__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_creatfp;
		}
		if(init_journal(inode)) {
			printk(KERN_ALERT "%s:%d init_journal Failed \n",
					__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_creatfp;
		}*/
	}else {
	      ei->isjourn = 0;
	      crfs_dbg_verbose("%s:%d journal disabled, \n",
			 __FUNCTION__,__LINE__);
        }
	if (!kernel_call && copy_to_user((void __user *)arg, &map, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d %lu\n",
				__FUNCTION__, __LINE__, minsz);
		retval = -EFAULT;
		goto err_creatfp;
	}else if (kernel_call) {
		mapptr->fd = fd;
	}

	return 0;

err_creatfp:
	return retval;
}
EXPORT_SYMBOL(vfio_creatfile_cmd);


static inline void w32(nvme_device_t* dev, u32* addr, u32 val)
{
	*addr = val;
}


int read_from_ioqueue(struct inode *inode, nvme_cmdrw_t *cmdrw, 
	struct crfss_fstruct **rd, struct crfss_fstruct **target_rd, 
	struct crfss_fstruct **origin_rd) 
{
	int tree_search = -1;
	int retval = -EFAULT;
	int isappend = 0;

	if (cmdrw->slba == -1) {
		isappend = 1;
		cmdrw->slba = (*rd)->fp->f_pos;	
	}

	tree_search = crfss_read_submission_tree_search(inode, cmdrw, target_rd);

	if (tree_search == DEVFS_SUBMISSION_TREE_FOUND) {
		/* Fast path: All the target block found in submission queue */
		retval = cmdrw->nlb;
	} else if (tree_search == DEVFS_SUBMISSION_TREE_PARFOUND) {
		/*
		 * Slow path: Only partial block found in submission queue
		 * Need to insert read request to the first found block
		 * rd queue
		 */
		if (!*target_rd) {
			printk(KERN_ALERT "%s:%d Target rd is NULL \n",
					__FUNCTION__, __LINE__);
			retval = -EFAULT;
		}
		if (*target_rd && *target_rd != *rd) {
			if (test_bit(0, &((*target_rd)->closed)) == 0 && 
				test_and_set_bit(1, &((*target_rd)->closed)) == 0) {

				*origin_rd = *rd;
				*rd = *target_rd;
			}
		}
	} else if (tree_search == DEVFS_SUBMISSION_TREE_NOTFOUND) {
		/* All the target block are in storage */
		retval = 0;
	} else {
		/* Error occured */
		retval = -EFAULT;
	}

	if (isappend == 1) {
		cmdrw->slba = -1;
		if (retval == cmdrw->nlb)
			(*rd)->fp->f_pos += retval;
	}

	return retval;	
}

/*
* Search for existing writes in the interval tree that are not yet committed 
* to disk and are currently queued 
*/
int search_for_conflicts(struct crfss_fstruct **target_rd, struct crfss_fstruct **origin_rd, 
	struct crfss_fstruct **rd, nvme_cmdrw_t *cmdrw) {

	int tree_search = 0;
	int retval = 0;

	tree_search = crfss_write_submission_tree_search((*rd)->fp->f_inode, cmdrw, target_rd);
	if (tree_search == DEVFS_SUBMISSION_TREE_FOUND) {
		/* A conflict write found
		 * add this request to that rd queue
		 * to ensure ordering
		 */

		if (*target_rd && *target_rd != *rd) {
			if (test_bit(0, &((*target_rd)->closed)) == 0 && 
				test_and_set_bit(1, &((*target_rd)->closed)) == 0) {
#ifdef _DEVFS_STAT
				crfss_stat_fp_queue_conflict();
#endif
				*origin_rd = *rd;
				*rd = *target_rd;
			}
		}

	} else if (tree_search == DEVFS_SUBMISSION_TREE_NOTFOUND) {
		/* No conflict write */
	} else {
		/* Error occured */
		retval = -EFAULT;
	}
	return retval;
}

int prepare_read(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw, 
		int *buf_idx)
{
	int retval = 0;
	void *ptr = (void *)cmdrw->common.prp2;

	if(!ptr) {
		printk(KERN_ALERT "%s, %d cmdrw->common.prp2 NULL \n",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto prepare_read_ret;
	}

	/* enqueue per file pointer submission queue */
	if (*buf_idx >= rd->qentrycnt) {
		printk(KERN_ALERT "%s, %d buffer is full, idx = %d \n",
			__FUNCTION__, __LINE__, *buf_idx);
		retval = -EFAULT;
		goto prepare_read_ret;
	}

#ifndef CRFS_SCHED_THREAD
	cmdrw->blk_addr = (__u64)get_data_buffer(rd, cmdrw);
	if (!cmdrw->blk_addr) {
		printk(KERN_ALERT "%s, %d Failed to allocate kernel buffer \n",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto prepare_read_ret;
	}
#else
	cmdrw->blk_addr = (__u64)cmdrw->common.prp2;
	//memset((void __user *)cmdrw->blk_addr, cmdrw->nlb, 0);
#endif

prepare_read_ret:
	return retval;
}


int prepare_write_append(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw, 
		int *buf_idx)
{
	int retval = 0;
	void *ptr = (void *)cmdrw->common.prp2;

	if(!ptr) {
		printk(KERN_ALERT "%s, %d cmdrw->common.prp2 NULL \n",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto prepare_write_ret;
	}

	/* enqueue per file pointer submission queue */
	if (*buf_idx >= rd->qentrycnt) {
		printk(KERN_ALERT "%s, %d buffer is full, idx = %d \n",
			__FUNCTION__, __LINE__, *buf_idx);
		retval = -EFAULT;
		goto prepare_write_ret;
	}

#if 1 //ndef CRFS_SCHED_THREAD
	/* Get kernel data buffer */
	cmdrw->blk_addr = (__u64)get_data_buffer(rd, cmdrw);
	if (!cmdrw->blk_addr) {
		printk(KERN_ALERT "%s, %d Failed to allocate kernel buffer \n",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto prepare_write_ret;
	}

	/* Copy data from user to device */
	if (copy_from_user((void *)cmdrw->blk_addr, (void __user *)ptr, cmdrw->nlb)) {
		printk(KERN_ALERT "page copy failed %lu\n", (unsigned long)ptr);
		retval = -EFAULT;
		goto prepare_write_ret;
	}
#else
	cmdrw->blk_addr = (void *)cmdrw->common.prp2;
	memset((void __user *)cmdrw->blk_addr, cmdrw->nlb, 0);
#endif

	/* Add index in per inode submission tree */
	if (crfss_write_submission_tree_insert(rd->fp->f_inode, cmdrw, rd)) {
		printk(KERN_ALERT "%s, %d Radix tree insert failed \n",
				__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto prepare_write_ret;
	}
	retval = cmdrw->nlb;

prepare_write_ret:
	return retval;
}


void enqueue_request (struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) 
{

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	crfss_mutex_lock(&rd->read_lock);
	rd->num_entries++;
	crfss_mutex_unlock(&rd->read_lock);

checkqueue:
	/*
	 * If this command is getting called after device thread is terminated in some
	 * applications like RocksDB, just return without spin as device thread is
	 * already terminated at this time
	 */
#ifdef CRFS_MULTI_PROC
	if (g_crfss_scheduler_init[process_idx] == 0)
#else
	if (g_crfss_scheduler_init == 0)
#endif
		return;

	BUG_ON(!rd);
	crfss_mutex_lock(&rd->read_lock);
	if (((rd->fifo.head + rd->entrysize) & (rd->queuesize - 1)) == rd->fifo.tail) {
		crfss_mutex_unlock(&rd->read_lock);
		goto checkqueue;
	}
	rd_enqueue(rd, sizeof(__u64), cmdrw);
	crfss_mutex_unlock(&rd->read_lock);
}


int finish_read(struct crfss_fstruct *rd, nvme_cmdrw_t *cmdrw) 
{
	int retval = 0;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	/* Slow path: Need to insert into request queue, wait for
	 * device thread to handle */
	while(test_and_clear_bit(0, (volatile long unsigned int *)&cmdrw->status) == 0) {
		/*
		 * If this command is getting called after device thread is terminated in some
		 * applications like RocksDB, just return without spin as device thread is
		 * already terminated at this time
		 */
#ifdef CRFS_MULTI_PROC
		if (g_crfss_scheduler_init[process_idx] == 0) {
#else
		if (g_crfss_scheduler_init == 0) {
#endif
			retval = cmdrw->nlb;
			goto finish_read_ret;
		}
	}

	/* Get return value from device thread */
	retval = cmdrw->ret;

#ifndef CRFS_SCHED_THREAD
	if (copy_to_user((void __user *)cmdrw->common.prp2,
			(void *)cmdrw->blk_addr, cmdrw->nlb)) {
		crfss_mutex_lock(&rd->read_lock);
		rd->num_entries--;
		crfss_mutex_unlock(&rd->read_lock);
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto finish_read_ret;
	}

	/* Release kernel data buffer */
	put_data_buffer(rd, cmdrw);

	cmdrw->blk_addr = NULL;
	kfree(cmdrw);
#endif

	crfss_mutex_lock(&rd->read_lock);
	rd->num_entries--;
	crfss_mutex_unlock(&rd->read_lock);

finish_read_ret:	
	return retval;
}

/* perform io using the file descriptor */
long vfio_submitio_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_rw_cmd map;
	unsigned long start_addr;
	nvme_cmdrw_t *cmdrw = NULL;
	long retval = 0;
	int i = 0, buf_idx = 0;
	uint32_t mask = VFIO_DMA_MAP_FLAG_READ |
			VFIO_DMA_MAP_FLAG_WRITE;
	int fd = -1;
	struct crfss_fstruct *rd = NULL;

#ifdef _DEVFS_SCALABILITY
	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;
	struct file *file = NULL;
	struct crfss_fstruct *origin_rd = NULL;
	struct crfss_fstruct *target_rd = NULL;

#endif

	minsz = offsetofend(struct vfio_crfss_rw_cmd, cmd_count);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_submit_io;
	}

	start_addr = (unsigned long)map.vaddr;
	if(!start_addr) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_submit_io;
	}

	cmdrw = kmalloc(sizeof(nvme_cmdrw_t), GFP_KERNEL);
	fd = map.fd;
	rd = fd_to_queuebuf(fd);
	/* make a copy of current rd */
	origin_rd = rd;

	/*file structure does not exist for exisiting files opened first time*/
	if(!rd) {
		rd = crfss_init_file_struct(g_qentrycnt);
		if(!rd) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			goto err_submit_io;
		}
	}

	if(!rd->fp) {
		//rd->fp = fget(fd);
		printk(KERN_ALERT "Null file pointer %s:%d \n",__FUNCTION__,__LINE__);
		goto err_submit_io;
	}

	if (map.argsz < minsz || map.flags & ~mask) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_submit_io;
	}

	file = rd->fp;
	if(!file) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		retval = -EFAULT;
		goto err_submit_io;
	}
	inode = file->f_inode;
	if(!inode) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		retval = -EFAULT;
		goto err_submit_io;
	}
	ei = DEVFS_I(inode);
	if(!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);	
		retval = -EFAULT;
		goto err_submit_io;
	}

#ifndef _DEVFS_KTHREAD_DISABLED
	if(inode->sq_tree_init != _DEVFS_INITIALIZE){
		printk(KERN_ALERT "%s:%d Radix tree NULL \n",__FUNCTION__, __LINE__);
		retval = -EFAULT; 
		goto err_submit_io;
	}
#endif

	for (i = 0; i < map.cmd_count; i++) {
		/* Copy cmd from user space */
		if (copy_from_user(cmdrw, (void __user*)start_addr, sizeof(nvme_cmdrw_t))) {
			    printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			    retval = -EFAULT;
			    goto err_submit_io;
		}

#ifndef _DEVFS_KTHREAD_DISABLED
		/* Check the command opcode type
		 * For reads, we currently do an immediate read by copying the
		 * the data into user space buffer.
		 * Multiple commands can be supported at the same time
		 */
		if (cmdrw->common.opc == nvme_cmd_write ||
		    cmdrw->common.opc == nvme_cmd_append) {
			/* 
			 * Write operations First step:
			 * Lookup per-inode submission tree, if an conflict write is ahead,
			 * insert to that queue instead of current queue
			 */
			retval = search_for_conflicts(&target_rd, &origin_rd, &rd, cmdrw);
			if (retval == -EFAULT)
				goto err_submit_io;	

			/* update TSC for current jiffies */
			rd->tsc = jiffies;
			cmdrw->cmdtsc = jiffies;

			/* prepare for write/append request */
			retval = prepare_write_append(rd, cmdrw, &buf_idx);
			if (retval == -EFAULT)
				goto err_submit_io;

			/* enqueue write/append request */
			enqueue_request (rd, cmdrw);

			/* 
			 * The purpose of this is to delay the close
			 * on this the new rd that frees data blocks
			 * of that rd
			 */
			if (origin_rd != rd) {
				test_and_clear_bit(1, &rd->closed);
			}

		} else if (cmdrw->common.opc == nvme_cmd_read) {
			/* Trying out performance tuning by 
			* disabling some code.DEVFS_PERF_TUNING is 
			* disabled otherwise
			*/
#ifndef CRFS_PERF_TUNING
			/* Read operations fast path 
			 * First check if the requested block is being updated
			 * by some other concurrent writers.
			 * If submission tree hit, return directly,
			 * otherwise, insert to its file pointer queue.
			 */
			retval = -EFAULT;
			retval = read_from_ioqueue(inode, cmdrw, &rd, &target_rd, &origin_rd);
			if (retval == -EFAULT)
				goto err_submit_io;
			if (retval == cmdrw->nlb)
				goto err_submit_io;
#endif //for CRFS_PERF_TUNING

			/* update TSC for current jiffies */
			rd->tsc = jiffies;
			cmdrw->cmdtsc = jiffies;

			/* prepare for read request */
			retval = prepare_read(rd, cmdrw, &buf_idx);

			if(retval == -EFAULT)
				goto err_submit_io;

			/* enqueue read request to FD-queue */
			enqueue_request (rd, cmdrw);

			/* 
			 * Slow path: Need to insert into request queue, wait for
			 * device thread to handle
			 */
			retval = finish_read(rd, cmdrw);	
			if (retval == -EFAULT)
				goto err_submit_io;

		} else if ( cmdrw->common.opc == nvme_cmd_lseek ) {
			// TODO
			//retval =  (long) crfss_llseek (rd,(void *)cmdrw);
		} else {
			printk(KERN_ALERT "%s:%d Invalid command op code = %d\n",
					__FUNCTION__,__LINE__,cmdrw->common.opc);
			// TODO
			retval = -EFAULT;
			goto err_submit_io;
		}

#else
		/* Host Thread Code, no device threads */
		if (cmdrw->common.opc == nvme_cmd_read) {
			if (cmdrw->slba == -1)
				retval = vfio_crfss_io_read_host_thread(rd, cmdrw, 1);
			else
				retval = vfio_crfss_io_read_host_thread(rd, cmdrw, 0);
		} else if (cmdrw->common.opc == nvme_cmd_write ||
			cmdrw->common.opc == nvme_cmd_append) {
			retval = vfio_crfss_io_append_host_thread(rd, cmdrw);
	
		} else if ( cmdrw->common.opc == nvme_cmd_lseek ) {
			// TODO
			//retval =  (long) crfss_llseek (rd,(void *)cmdrw);
		} else {
			printk(KERN_ALERT "%s:%d Invalid command op code = %d\n",
					__FUNCTION__,__LINE__,cmdrw->common.opc);
			// TODO
			retval = -EFAULT;
			goto err_submit_io;
		}
#endif	//_DEVFS_KTHREAD_DISABLED
	}
err_submit_io:
	return retval;
}
EXPORT_SYMBOL(vfio_submitio_cmd);


/* Close DEVFS file and its in-memory file structures */
int crfss_close(struct inode *inode, struct file *fp){

	int retval = 0;
	struct crfss_fstruct *rd = NULL;
	struct crfss_inode *ei = NULL;

	rd = fp->cmdbuf;
	if(!rd) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_rel;
	}

	/*Get DevFS inode info from inode*/
	ei = DEVFS_I(fp->f_inode);
	if(!ei) {
		printk(KERN_ALERT "%s:%d DevFS inode info NULL \n",
				__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_rel;
	}

	/* Release in-memory file structures */
	if (crfss_free_file_queue(rd)) {
		printk(KERN_ALERT "Failed: DevFS cleanup %s:%d \n",
				__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto err_rel;
	}

#ifndef _DEVFS_KTHREAD_DISABLED
	crfss_scalability_close_setup(ei, rd);
#endif
	//Free the file structure during close
	/*if (rd) {
		kfree(rd);
		fp->cmdbuf = NULL;
		rd = NULL;
	}*/

	//fsnotify_close(fp);
	//fput(fp);
	fp->f_pos = 0;
	filp_close(fp, NULL);
	//put_unused_fd(rd->fd);
	/*if (rd->task == NULL) {
		filp_close(fp, NULL);
		return 0;
	}
	__close_fd(rd->task->files, rd->fd);*/

err_rel:
	return retval;
}

/* Close DEVFS file and its in-memory file structures */
int vfio_crfss_io_close(struct crfss_fstruct *rd) {
	int retval = 0, fd = -1;
	struct file *fp = NULL;
	struct crfss_sb_info *sbi = NULL;

	fp = rd->fp;
	if (!fp) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_io_close;
	}

	BUG_ON(!fp->isdevfs);
	BUG_ON(!fp->f_inode);

	sbi = DEVFS_SB(fp->f_inode->i_sb);

	crfss_dbgv("%s:%d fd %d, inodeoffz %lu\n", __FUNCTION__,__LINE__,
			fd, sbi->inodeoffsz);

	if (crfss_close(fp->f_inode, fp)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_io_close;
	}

	/*printk(KERN_ALERT "fd = %d closed | %s:%d\n", 
		rd->fd, __FUNCTION__, __LINE__);*/

err_io_close:
	return retval;
}

#ifndef _DEVFS_KTHREAD_DISABLED
int vfio_close_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_closefd_cmd map;
	int retval = 0, fd = -1;
	struct file *fp = NULL;
	struct crfss_fstruct *rd = NULL;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	nvme_cmdrw_t kcmdrw;
	nvme_cmdrw_t *cmdrw = &kcmdrw;
	memset(cmdrw, 0, sizeof(nvme_cmdrw_t));

	cmdrw->common.opc = nvme_cmd_close;
	cmdrw->status = 0;

#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_closefd_cmd, fd);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_closefd;
	}

	fd = map.fd;
	//fp = fget(fd);

	rd = fd_to_queuebuf(fd);
	if (!rd) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		goto err_closefd;
	}
	if (!rd->fp) {
		printk(KERN_ALERT "Null file pointer %s:%d \n",__FUNCTION__,__LINE__);
		goto err_closefd;
	}
	fp = rd->fp;
	BUG_ON(!fp->isdevfs);
	BUG_ON(!fp->f_inode);

#ifdef CRFS_OPENCLOSE_OPT
	if(crfss_close(fp->f_inode, fp)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_closefd;
	}
	return retval;
#endif

	/*
	 * If close is getting called after device thread is terminated in some
	 * applications like RocksDB, just release its resource without spinning
	 * as device thread is already terminated at this time
	 */
#ifdef CRFS_MULTI_PROC
	if (g_crfss_scheduler_init[process_idx] == 0) {
#else
	if (g_crfss_scheduler_init == 0) {
#endif
		if (crfss_close(fp->f_inode, fp)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_closefd;
		}
		kfree(rd);
		rd = NULL;
		return retval;
	}

	/* mark this file pointer as being closed,
	 * to prevent conflict write joining this
	 * rd */
	while (test_and_set_bit(1, &rd->closed) == 1);

#ifdef _DEVFS_FSYNC_ENABLE
	/* loop until current fsync is done */
	while (test_bit(0, (volatile long unsigned int *)&fp->f_inode->fsync_barrier) == 1);	
#endif
	test_and_set_bit(0, &rd->closed);

checkqueue:
	crfss_mutex_lock(&rd->read_lock);
	if (((rd->fifo.head + rd->entrysize) & (rd->queuesize - 1)) == rd->fifo.tail) {
		crfss_mutex_unlock(&rd->read_lock);
		goto checkqueue;
	}
	rd_enqueue(rd, sizeof(__u64), cmdrw);
	crfss_mutex_unlock(&rd->read_lock);

	while(test_and_clear_bit(0, (volatile long unsigned int *)&cmdrw->status) == 0) {
		/*
		 * If this command is getting called after device thread is terminated in some
		 * applications like RocksDB, just return without spin as device thread is
		 * already terminated at this time
		 */
#ifdef CRFS_MULTI_PROC
		if (g_crfss_scheduler_init[process_idx] == 0) {
#else
		if (g_crfss_scheduler_init == 0) {
#endif
			retval = cmdrw->nlb;
			break;
		}
	}

err_closefd:
	return retval;
}
#else

int vfio_close_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_closefd_cmd map;
	int retval = 0, fd = -1;
	struct file *fp = NULL;
	struct crfss_sb_info *sbi = NULL;
	struct crfss_fstruct *rd = NULL;

#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_closefd_cmd, fd);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_closefd;
	}

	fd = map.fd;
	//fp = fget(fd);

	rd = fd_to_queuebuf(fd);
	if (!rd) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		goto err_closefd;
	}
	if (!rd->fp) {
		printk(KERN_ALERT "Null file pointer %s:%d \n",__FUNCTION__,__LINE__);
		goto err_closefd;
	}
	fp = rd->fp;

	BUG_ON(!fp->isdevfs);
	BUG_ON(!fp->f_inode);

	sbi = DEVFS_SB(fp->f_inode->i_sb);

	crfss_dbgv("%s:%d fd %d, inodeoffz %lu\n", __FUNCTION__,__LINE__, 
			fd, sbi->inodeoffsz);


	if(crfss_close(fp->f_inode, fp)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_closefd;
	}

#ifdef _DEVFS_SCALABILITY_DBG
	struct inode *inode = fp->f_inode;
	printk(KERN_ALERT "inode=%llx, inode->i_size=%llu\n",
			(__u64)inode, inode->i_size);
#endif
	//printk(KERN_ALERT "Closing fd = %d | %s:%d\n", fd, __FUNCTION__, __LINE__);


err_closefd:
	return retval;
}
#endif

/* 
 * initialize firmware level file system 
 */
static int vfio_creatfs_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_creatfs_cmd map;
	int retval = 0;
	uint32_t mask = VFIO_DMA_MAP_FLAG_READ |
			VFIO_DMA_MAP_FLAG_WRITE;
	u8 cred_id[CRED_ID_BYTES];

#ifdef _DEVFS_DEBUG_ENT
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_creatfs_cmd, sched_policy);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_creatfs;
	}

	/*start_addr = (unsigned long)map.vaddr;
		if(!start_addr) {
		      printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		      retval = -EFAULT;
		      goto err_creatfs;
		}*/

	if (map.argsz < minsz || map.flags & ~mask) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_creatfs;
	}

	/* Generate an unique process id */
	get_random_bytes(cred_id, CRED_ID_BYTES);
	memcpy(map.cred_id, cred_id, CRED_ID_BYTES);
	crfss_add_cred_table(cred_id);

	if (copy_to_user((void __user *)arg, &map, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d %lu\n",
			__FUNCTION__, __LINE__, minsz);
		retval = -EFAULT;
	}

#ifndef CRFS_SCHED_THREAD
#ifndef _DEVFS_KTHREAD_DISABLED

	if (!g_crfss_scheduler_init) {	
		/* 
		 * Initialize I/O scheduler
		 * Create device threads to handle I/O requests
		 */
		crfss_scheduler_init(map.dev_core_cnt, map.sched_policy);
	}
#endif //_DEVFS_KTHREAD_DISABLED
#endif //CRFS_SCHED_THREAD

	if(g_crfss_init) {
#if defined(_DEVFS_DEBUG)
		printk(KERN_ALERT "DEBUG: %s:%d DevFS already initialized \n",
			__FUNCTION__,__LINE__);
#endif
		goto err_creatfs;
	}

	/*Create inode tree list */
	if(!g_crfss_init)
		g_inotree = crfss_inode_list_create();

	/*Initialize DevFS file system emulation*/
	g_crfss_init = 1;

err_creatfs:
	return retval;
}


/*
 * nvmed_get_buffer_addr
 * translate virtual address to physical address, set reserved flags
 */
static int vfio_creatq_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_iommu_type1_queue_map map;
	struct task_struct *task = current;
	struct mm_struct *mm;
	unsigned long vaddr, start_addr;
	int i=0, retval=0;
	__u64* qpfnlist;
	struct page *page_info;

	pgd_t *pgd;
	pte_t *ptep, pte;
	pud_t *pud;
	pmd_t *pmd;

	uint32_t mask = VFIO_DMA_MAP_FLAG_READ |
			VFIO_DMA_MAP_FLAG_WRITE;

	crfs_dbg("Enter Calling %s:%d \n",__FUNCTION__,__LINE__);

	minsz = offsetofend(struct vfio_iommu_type1_queue_map, qpfns);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto ret_qmap;
	}

	start_addr = (unsigned long)map.vaddr;
	if(!start_addr) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto ret_qmap;
	}

	if (map.argsz < minsz || map.flags & ~mask) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto ret_qmap;
	}
	if (map.qpfns > _DEVFS_QUEUE_PAGES) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d %llu\n",__FUNCTION__,__LINE__, map.qpfns);
		retval = -EFAULT;
		goto ret_qmap;
	}

	qpfnlist = kzalloc(sizeof(__u64) * map.qpfns, GFP_KERNEL);
	if(!qpfnlist) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d %llu\n",__FUNCTION__,__LINE__, map.qpfns);
		retval = -EFAULT;
		goto ret_qmap;
	}
	mm = task->mm;
	if(!mm) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto ret_qmap;
	}
	down_read(&mm->mmap_sem);

	crfs_dbg("vfio_creatq_cmd page mapping pages %llu\n",map.qpfns);
#ifdef CRFS_OPENCLOSE_OPT
	dma_able_user_addr = start_addr;
#endif

	for(i=0; i<map.qpfns; i++) {

		vaddr = start_addr + (PAGE_SIZE * i);

		pgd = pgd_offset(mm, vaddr);
		if(pgd_none(*pgd) || pgd_bad(*pgd)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			break;
		}
		pud = pud_offset(pgd, vaddr);
		if(pud_none(*pud) || pud_bad(*pud)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			break;
		}
		pmd = pmd_offset(pud, vaddr);
		if(!pmd_none(*pmd) &&
				(pmd_val(*pmd) & (_PAGE_PRESENT|_PAGE_PSE)) != _PAGE_PRESENT) {
			pte = *(pte_t *)pmd;
			qpfnlist[i] = (pte_pfn(pte) << PAGE_SHIFT) + (vaddr & ~PMD_MASK);
			continue;
		}
		else if (pmd_none(*pmd) || pmd_bad(*pmd)) {
			retval = -EFAULT;
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			break;
		}

		ptep = pte_offset_map(pmd, vaddr);
		if(!ptep) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			break;
		}

		pte = *ptep;
		page_info = pte_page(pte);
		qpfnlist[i] = pte_pfn(pte) << PAGE_SHIFT;
		pte_unmap(ptep);

#ifdef CRFS_OPENCLOSE_OPT
		dma_able_fd_queue_map[i] = (__u64)page_address(page_info);
#endif
	}
	up_read(&mm->mmap_sem);

	/*if(!retval && qpfnlist) {
		    map.qpfnlist = qpfnlist;
		}*/

	if (copy_to_user(map.qpfnlist, qpfnlist, sizeof(__u64)*map.qpfns)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d %llu\n",__FUNCTION__,__LINE__,
				sizeof(__u64)*map.qpfns);
		retval = -EFAULT;
	}
	if (qpfnlist) {
		kfree(qpfnlist);
		qpfnlist = NULL;
	}
ret_qmap:
	return retval;
}

/* 
 * close firmware level file system 
 * exit all device threads
 */
static int vfio_closefs_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_closefs_cmd map;
	int retval = 0;

#ifdef _DEVFS_DEBUG_ENT
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_closefs_cmd, argsz);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_closefs;
	}

	if (map.argsz < minsz) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_closefs;
	}

	/* Exit device threads that handles I/O requests */
#ifndef _DEVFS_KTHREAD_DISABLED
	crfss_scheduler_exit();
#endif

	/* Remove cred id from credential table */
	crfss_del_cred_table(map.cred_id);	

err_closefs:
	return retval;
}


/* DEVFS fsync */
int vfio_fsync_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_fsync_cmd map;
	int retval = 0;
	int fd = -1, rd_nr = 0;
	int i = 0;
	struct file *fp = NULL;
	struct crfss_fstruct *rd = NULL;
	struct inode *inode = NULL;	
	struct crfss_inode *ei = NULL;
	struct crfss_fstruct *cur_rd;
	nvme_cmdrw_t *cmdrw = NULL;

#ifndef _DEVFS_FSYNC_ENABLE
	return 0;
#endif

#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_fsync_cmd, fd);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_fsync;
	}

	fd = map.fd;

	rd = fd_to_queuebuf(fd);
	if (!rd) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		goto err_fsync;
	}
	if (!rd->fp) {
		printk(KERN_ALERT "Null file pointer %s:%d \n",__FUNCTION__,__LINE__);
		goto err_fsync;
	}
	fp = rd->fp;

	BUG_ON(!fp->isdevfs);

        inode = fp->f_inode;
        if (!inode) {
                printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);    
                retval = -EFAULT;
                goto err_fsync;
        }    

        ei = DEVFS_I(inode);
        if (!ei) {
                printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);    
                retval = -EFAULT;
                goto err_fsync;
        }

	//printk(KERN_ALERT "fsync starts rd = %llx!\n", rd);

test_fsync_cond:
	if (test_and_set_bit(0, (volatile long unsigned int *)&inode->fsync_barrier) == 0) {
		read_lock(&ei->i_meta_lock);
		/* 
		 * fsync is not issued by other threads simoutaneously, 
		 * adding fsync cmd to all the FD-queues
		 */
		rd_nr = ei->rd_nr;

		/* Atomically set fsync barrier counter to # of fds */
		atomic_set(&inode->fsync_counter, rd_nr);

		/* Iterate all the active FD-queues */
		for (i = 0; i < MAX_FP_QSIZE; ++i) {
			cur_rd = ei->per_rd_queue[i];
			if (!cur_rd)
				continue;

			/*
			 * If this FD-queue already has a close operation in the
			 * queue and being processed, then their is no point of
			 * adding a fsync barrier to this FD-queue
			 */
			if (test_bit(0, &cur_rd->closed) == 1) {
				atomic_dec(&inode->fsync_counter);
				continue;
			}

			/* Allocate a new fsync barrier cmd */
			cmdrw = kmalloc(sizeof(nvme_cmdrw_t), GFP_KERNEL);
		        if (!cmdrw) {
			        printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);    
				retval = -EFAULT;
				goto err_fsync;
			}
			cmdrw->common.opc = nvme_cmd_flush;	

			/* Insert fsync barrier cmd to this FD-queue */
			enqueue_request(cur_rd, cmdrw);
		}
		read_unlock(&ei->i_meta_lock);

		/*printk(KERN_ALERT "init counter = %d, rd = %llx\n",
				atomic_read(&inode->fsync_counter), rd);*/

	} else
		goto test_fsync_cond;

	/* loop until fsync barrier flag is cleared */
	while (test_bit(0, (volatile long unsigned int *)&inode->fsync_barrier) == 1);

	//printk(KERN_ALERT "fsync return\n");

err_fsync:
	return retval;
}


/* DEVFS unlink */
int vfio_crfss_io_unlink (struct crfss_fstruct *rd) {
	int retval = 0;
	char *p = (__force char __user *)rd->fname;

	//printk(KERN_ALERT "call unlink in kthread\n");

	retval = do_crfss_unlink((const char __user *)p);
	if (retval) {
		printk(KERN_ALERT "DEBUG: crfss_unlink failed, retval = %d | %s:%d \n",
				retval,__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_io_unlink;
	}

	/* Remove current request from queue when finish */ 
	//crfss_mutex_lock(&rd->read_lock);
	rd_dequeue(rd, sizeof(__u64));
	//crfss_mutex_unlock(&rd->read_lock);

err_io_unlink:
	return retval;
}

#if 0
int vfio_unlink_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_unlink_cmd map;
	int retval = 0, i = 0, rd_nr = 0;

	struct inode *inode = NULL;
	struct dentry *dentry;
	struct path path;
	struct crfss_inode *ei = NULL;
	struct crfss_fstruct *rd = &control_rd;
	char pathname[NAME_MAX];

	nvme_cmdrw_t kcmdrw;
	nvme_cmdrw_t *cmdrw = &kcmdrw;

	cmdrw->common.opc = nvme_cmd_unlink;

#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_unlink_cmd, uptr);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_unlink;
	}

	memset(pathname, 0, NAME_MAX);
	copy_from_user(pathname, (void __user *)map.uptr, NAME_MAX);

	kern_path(pathname, LOOKUP_FOLLOW, &path);
	dentry = path.dentry;
	inode = dentry->d_inode;

	ei = DEVFS_I(inode);
	if (!ei) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto err_unlink;
	}

	/*printk(KERN_ALERT "DEBUG: %s:%d filename = %s, ei = %llx, inode = %llx\n",
		__FUNCTION__, __LINE__, pathname, ei, inode);*/

	memset(rd->fname, 0, NAME_MAX);
	memcpy(rd->fname, pathname, strlen(pathname));

	/* update TSC for current jiffies */
	cmdrw->cmdtsc = jiffies;

checkqueue:
	crfss_mutex_lock(&rd->read_lock);
	if (((rd->fifo.head + rd->entrysize) & (rd->queuesize - 1)) == rd->fifo.tail) {
		crfss_mutex_unlock(&rd->read_lock);
		goto checkqueue;
	}
	rd_enqueue(rd, sizeof(__u64), cmdrw);
	crfss_mutex_unlock(&rd->read_lock);


	while(test_and_clear_bit(0, &cmdrw->status) == 0);

err_unlink:
	return retval;
}
#endif

//#if 0
int vfio_unlink_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_unlink_cmd map;
	int retval = 0;

#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_unlink_cmd, uptr);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_unlink;
	}

	retval = do_crfss_unlink((const char __user *)map.uptr);
	if (retval) {
		printk(KERN_ALERT "DEBUG: crfss_unlink failed, retval = %d | %s:%d \n",
				retval,__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_unlink;
	}

err_unlink:
	return retval;
}
//#endif

int vfio_rename_cmd(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_rename_cmd map;
	int retval = 0;

	/*struct inode *inode = NULL;
	struct dentry *dentry;
	struct path path;
	struct crfss_inode *ei = NULL;*/

	nvme_cmdrw_t kcmdrw;
	nvme_cmdrw_t *cmdrw = &kcmdrw;

	cmdrw->common.opc = nvme_cmd_rename;

#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_rename_cmd, newname);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_rename;
	}

	sys_renameat2(AT_FDCWD, (void __user *)map.oldname,
					AT_FDCWD, (void __user *)map.newname, 0);

err_rename:
	return retval;
}

int vfio_init_devthread(unsigned long arg){

	unsigned long minsz;
	struct vfio_crfss_init_devthread_cmd map;
	int retval = 0, dev_thread_nr = 1, thread_idx = 0;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif


#if defined(_DEVFS_DEBUG_ENT)
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif

	minsz = offsetofend(struct vfio_crfss_init_devthread_cmd, sched_policy);

	if (copy_from_user(&map, (void __user *)arg, minsz)) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_init_devthread;
	}
	dev_thread_nr = map.dev_core_cnt;
	thread_idx = map.thread_index;

#ifdef CRFS_MULTI_PROC
	if (g_crfss_scheduler_init[process_idx] == 0) {
		crfss_scheduler_init(dev_thread_nr, map.sched_policy);
	}

	crfss_io_scheduler(&crfss_device_thread[process_idx][thread_idx]);
#else
	if (g_crfss_scheduler_init == 0) {
		crfss_scheduler_init(dev_thread_nr, map.sched_policy);
	}

	crfss_io_scheduler(&crfss_device_thread[thread_idx]);
#endif

err_init_devthread:
	return retval;
}


long crfss_ioctl(void *iommu_data, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	//printk("rel = %d, cmd = %d start\n", VFIO_IOMMU_GET_QUEUE_ADDR, cmd);
	//crfss_mutex_lock(&temp_mutex);

	if (cmd == VFIO_IOMMU_GET_QUEUE_ADDR) {
		crfs_dbg("DEVFS: ioctl VFIO_IOMMU_GET_QUEUE_ADDR\n");
		ret = vfio_creatq_cmd(arg);
	} else if (cmd ==  VFIO_DEVFS_RW_CMD) {
		ret = vfio_submitio_cmd(arg);
	} else if (cmd == VFIO_DEVFS_FSYNC_CMD) {
		ret = vfio_fsync_cmd(arg);
	} else if (cmd ==  VFIO_DEVFS_CREATFS_CMD) {
		crfs_dbg("DEVFS: ioctl VFIO_DEVFS_CREATFS_CMD\n");
		ret = vfio_creatfs_cmd(arg);
	} else if (cmd == VFIO_DEVFS_CREATFILE_CMD) {
		ret = vfio_creatfile_cmd(arg, 0);
	} else if (cmd == VFIO_DEVFS_CLOSEFILE_CMD) {
		ret = vfio_close_cmd(arg);
	} else if (cmd == VFIO_DEVFS_CLOSEFS_CMD) {
		ret = vfio_closefs_cmd(arg);
	} else if (cmd == VFIO_DEVFS_SUBMIT_CMD) {
		//return crfss_submit_cmd(arg, iommu_data);
	} else if (cmd == VFIO_DEVFS_UNLINK_CMD) {
		ret = vfio_unlink_cmd(arg);
	} else if (cmd == VFIO_DEVFS_RENAME_CMD) {
		ret = vfio_rename_cmd(arg);
	} else if (cmd == VFIO_DEVFS_INIT_DEVTHREAD_CMD) {
		ret = vfio_init_devthread(arg);
	}

	//printk("cmd = %d end\n", cmd);
	//crfss_mutex_unlock(&temp_mutex);

	return ret;
}
EXPORT_SYMBOL(crfss_ioctl);


static int __init init_crfss_fs(void)
{
	int err = init_crfss_inodecache();
	if (err)
		goto out1;

	//if (test_and_set_bit(0, &once))
	printk(KERN_ALERT "init_crfss_fs called \n");


	/*Create inode tree list */
	//g_inotree = crfss_inode_list_create();


	err = register_filesystem(&crfss_fs_type);
	if (err)
		goto out;

	/*if(!g_inotree) {
		printk(KERN_ALERT "Failed %s:%d \n",__FUNCTION__,__LINE__);
		goto out1;	
	}*/	
	printk(KERN_ALERT "%s:%d Success \n", __FUNCTION__,__LINE__);

	return 0;
	out:
	//destroy_inodecache();
	out1:
	printk(KERN_ALERT "%s:%d failed \n", __FUNCTION__,__LINE__);
	return err;
}
module_init(init_crfss_fs);

//EXPORT_SYMBOL(init_crfss_fs);
//fs_initcall(init_crfss_fs);

static void __exit unregister_crfss_fs(void)
{
	if (unregister_filesystem(&crfss_fs_type)) {
		printk(KERN_ALERT "%s:%d Failed \n", __FUNCTION__,__LINE__);
	}
	destroy_crfss_inodecache();
}
//EXPORT_SYMBOL(unregister_crfss_fs);
module_exit(unregister_crfss_fs);

MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);




