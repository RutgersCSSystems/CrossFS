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
 *
 * change log:
 * inode->i_mutex -> inode->i_rwsem
 * file-opeartion->{aio_read, aio_write} no longer exist
 * generic_write_sync API changes
 *
 * Derived from original ramfs:
 *
 * TODO: DEVFS description
 */

/*TODO: Header cleanup*/
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/devfs_def.h>
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
#include <linux/buffer_head.h>
#include <linux/namei.h>

#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>
#include <xen/xen.h>
#include <xen/interface/xen.h>
#include <xen/interface/memory.h>
#include <xen/balloon.h>
#include <xen/heteromem.h>
#include <xen/features.h>
#include <xen/page.h>
#include "pmfs.h"

extern long crfss_ioctl(void *iommu_data, 
		unsigned int cmd, unsigned long arg);

extern struct file *do_filp_open_new(int dfd, struct filename *pathname,
                const struct open_flags *op, int flags);

extern int file_read_actor(read_descriptor_t * desc, struct page *page, 
		unsigned long offset, unsigned long size);

extern int rw_verify_area(int read_write, struct file *file, const loff_t *ppos, size_t count);


#if 1 //def _DEVFS_NOVA_BASED
unsigned int crfss_blk_type_to_shift[DEVFS_BLOCK_TYPE_MAX] = {12, 21, 30};
uint32_t crfss_blk_type_to_size[DEVFS_BLOCK_TYPE_MAX] = {0x1000, 0x200000, 0x40000000};
#endif


#define RAMFS_DEFAULT_MODE      0755

//#define _USE_RAMFS

#if defined(_USE_RAMFS)
const struct address_space_operations crfss_aops = {
        .readpage       = simple_readpage,
        .write_begin    = simple_write_begin,
        .write_end      = simple_write_end,
        .set_page_dirty = __set_page_dirty_no_writeback,
};

const struct file_operations crfss_file_operations = {
	.open 		= do_crfss_open,
        .read           = new_sync_read,
	.read_iter	= generic_file_read_iter,
        //.aio_read       = generic_file_aio_read,
        .write          = new_sync_write,
	.write_iter	= generic_file_write_iter,
        //.aio_write      = generic_file_aio_write,
        .mmap           = generic_file_mmap,
        .fsync          = noop_fsync,
        .splice_read    = generic_file_splice_read,
        .splice_write   = generic_file_splice_write,
        .llseek         = generic_file_llseek,
	.unlocked_ioctl = crfss_ioctl,
};

const struct inode_operations crfss_file_inode_operations = {
        .setattr        = simple_setattr,
        .getattr        = simple_getattr,
};

static const struct inode_operations crfss_dir_inode_operations = {
        .create         = crfss_create,
        .lookup         = simple_lookup,
        .link           = simple_link,
        .unlink         = simple_unlink,
        .symlink        = crfss_symlink,
        .mkdir          = crfss_mkdir,
        .rmdir          = simple_rmdir,
        .mknod          = crfss_mknod,
        .rename         = simple_rename,
};

#else

static const struct address_space_operations crfss_aops = {

        //.readpage       = simple_readpage,
        //.write_begin    = simple_write_begin,
        //.write_end      = simple_write_end,
        .set_page_dirty = __set_page_dirty_no_writeback,
	.readpage	= crfss_readpage,
	.write_begin	= crfss_write_begin,
	.write_end	= crfss_write_end,
	//.writepages     = crfss_writepages,
	//.set_page_dirty	= crfss_set_node_page_dirty,
};

const struct file_operations crfss_file_operations = {
        .read           = new_sync_read,
        //.aio_read       = generic_file_aio_read,
        .write          = new_sync_write,
        //.aio_write      = crfss_file_aio_write,
        //.mmap           = generic_file_mmap,
        .fsync          = noop_fsync,
        //.splice_read    = generic_file_splice_read,
        //.splice_write   = generic_file_splice_write,
        .llseek         = generic_file_llseek,
        //.release      = crfss_close,
        //.unlocked_ioctl = crfss_ioctl,
};

const struct inode_operations crfss_file_inode_operations = {
        .setattr        = simple_setattr,
        .getattr        = simple_getattr,
	//.rmdir		= crfss_rmdir,
	//.permission     = crfss_permission,
};

static const struct inode_operations crfss_dir_inode_operations = {
	.create		= crfss_create,
	.lookup		= simple_lookup,
	.link		= simple_link,
	.unlink		= crfss_unlink,
	.symlink	= crfss_symlink,
	.mkdir		= crfss_mkdir,
	.rmdir		= crfss_rmdir,
	.mknod		= crfss_mknod,
	.rename		= simple_rename,
	.atomic_open 	= 0,
};

static const struct super_operations crfss_ops = {
	//.statfs		= simple_statfs,
	.drop_inode	= generic_delete_inode,
	.show_options	= generic_show_options,
	.alloc_inode    = crfss_alloc_inode,
	.destroy_inode  = crfss_destroy_inode,
	//.evict_inode    = crfss_evict_inode,
};
#endif

#if 0
/**
 * page_cache_sync_readahead - generic file readahead
 * @mapping: address_space which holds the pagecache and I/O vectors
 * @ra: file_ra_state which holds the readahead state
 * @filp: passed on to ->readpage() and ->readpages()
 * @offset: start offset into @mapping, in pagecache page-sized units
 * @req_size: hint: total size of the read which the caller is performing in
 *            pagecache pages
 *
 * page_cache_sync_readahead() should be called when a cache miss happened:
 * it will submit the read.  The readahead logic may decide to piggyback more
 * pages onto the read request if access patterns suggest it will improve
 * performance.
 */
void page_cache_sync_readahead(struct address_space *mapping,
                               struct file_ra_state *ra, struct file *filp,
                               pgoff_t offset, unsigned long req_size)
{
        /* no read-ahead */
        if (!ra->ra_pages)
                return;

        /* be dumb */
        if (filp && (filp->f_mode & FMODE_RANDOM)) {
                force_page_cache_readahead(mapping, filp, offset, req_size);
                return;
        }

        /* do read-ahead */
        crfss_ondemand_readahead(mapping, ra, filp, false, offset, req_size);
}
EXPORT_SYMBOL_GPL(page_cache_sync_readahead);
#endif

int do_crfss_open(struct inode *inode, struct file *file)
{
        if (inode->i_private)
                file->private_data = inode->i_private;

	if(inode->crfss_host) {
		printk(KERN_ALERT "inode is currently using host memory \n");
	}else {
		printk(KERN_ALERT "inode is currently using device memory \n");
	}
        return 0;
}


/**
 * do_generic_file_read - generic file read routine
 * @filp:	the file to read
 * @ppos:	current file position
 * @desc:	read_descriptor
 * @actor:	read method
 *
 * This is a generic file read routine, and uses the
 * mapping->a_ops->readpage() function for the actual low-level stuff.
 *
 * This is really ugly. But the goto's actually try to clarify some
 * of the logic when it comes to error handling etc.
 */
static void crfss_generic_file_read(struct file *filp, loff_t *ppos,
		read_descriptor_t *desc, read_actor_t actor)
{
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct file_ra_state *ra = &filp->f_ra;
	pgoff_t index;
	pgoff_t last_index;
	pgoff_t prev_index;
	unsigned long offset;      /* offset into pagecache page */
	unsigned int prev_offset;
	int error;
	struct crfss_inode *ei = NULL;

	index = *ppos >> PAGE_CACHE_SHIFT;
	prev_index = ra->prev_pos >> PAGE_CACHE_SHIFT;
	prev_offset = ra->prev_pos & (PAGE_CACHE_SIZE-1);
	last_index = (*ppos + desc->count + PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT;
	offset = *ppos & ~PAGE_CACHE_MASK;
	ei = DEVFS_I(inode);

	//printk(KERN_ALERT "%s:%d \n",__FUNCTION__,__LINE__); 
	for (;;) {
		struct page *page;
		pgoff_t end_index;
		loff_t isize;
		unsigned long nr, ret;

#if !defined(_DEVFS_CHECK)
		cond_resched();
#endif

find_page:
#if defined(_DEVFS_HETERO)
      		page = hetero_alloc_IO(
			GFP_HIGHUSER_MOVABLE |__GFP_ZERO, 0, 0);
		if (page){
			SetPageReadahead(page);	
		}else {
#endif
			page = crfss_find_get_page(mapping, index, ei);
			if (!page) {

				printk(KERN_ALERT "get page %s:%d Failed\n",__FUNCTION__,__LINE__); 

				/*page_cache_sync_readahead(mapping,
						ra, filp,
						index, last_index - index);
				page = crfss_find_get_page(mapping, index, ei);*/
				if (unlikely(page == NULL))
					goto no_cached_page;
			}
#if defined(_DEVFS_HETERO)
		}
#endif

#if !defined(_DEVFS_CHECK)
		if (PageReadahead(page)) {

			crfss_page_cache_async_readahead(mapping,
					ra, filp, page,
					index, last_index - index);
		}
		if (!PageUptodate(page)) {

			printk(KERN_ALERT "%s:%d Failed\n",
				__FUNCTION__,__LINE__); 

			if (inode->i_blkbits == PAGE_CACHE_SHIFT ||
					!mapping->a_ops->is_partially_uptodate)
				goto page_not_up_to_date;

#ifdef _USELOCK
			if (!trylock_page(page))
				goto page_not_up_to_date;
#endif
			/* Did it get truncated before we got the lock? */
			if (!page->mapping)
				goto page_not_up_to_date_locked;
			if (!mapping->a_ops->is_partially_uptodate(page,
								desc, offset))
				goto page_not_up_to_date_locked;
#ifdef _USELOCK
			unlock_page(page);
#endif

		}
#endif

page_ok:
		/*
		 * i_size must be checked after we know the page is Uptodate.
		 *
		 * Checking i_size after the check allows us to calculate
		 * the correct value for "nr", which means the zero-filled
		 * part of the page is not copied back to userspace (unless
		 * another truncate extends the file - this is desired though).
		 */
		isize = i_size_read(inode);
		end_index = (isize - 1) >> PAGE_CACHE_SHIFT;

#ifdef _DEVFS_SCALABILITY_DBG
		printk(KERN_ALERT "inode = %llx, isize = %llu, inode->i_size = %llu, index = %lu, end_index=%lu fpos = %llu \n", 
				(__u64)inode, isize, inode->i_size, index, end_index, *ppos);
#endif
		if (unlikely(!isize || index > end_index)) {

			printk(KERN_ALERT "%s:%d Failed\n",
				__FUNCTION__,__LINE__); 

			page_cache_release(page);
			goto out;
		}


		/////////////////////////////////////////////////////////////////////
#ifdef _DEVFS_CHECK_PAGE_CONTENT

#ifdef _DEVFS_VERBOSE
		printk(KERN_ALERT "++++++ read pfn: %d\n", page_to_pfn(page)); 
#endif

		char *kaddr;

		kaddr = kmap_atomic(page);

#ifdef _DEVFS_VERBOSE
		printk(KERN_ALERT "$$$$ Target: %s\n", kaddr);
#endif

		kunmap_atomic(kaddr);
#endif
		//////////////////////////////////////////////////////////////////


		/* nr is the maximum number of bytes to copy from this page */
		nr = PAGE_CACHE_SIZE;
		if (index == end_index) {

			/*printk(KERN_ALERT "%s:%d Failed index %d, " 
				"end_index %d\n",__FUNCTION__,__LINE__, 
				index, end_index); */

			nr = ((isize - 1) & ~PAGE_CACHE_MASK) + 1;
			if (nr <= offset) {
				page_cache_release(page);
				goto out;
			}
		}
		nr = nr - offset;

#if !defined(_DEVFS_CHECK)
		/* If users can be writing to this page using arbitrary
		 * virtual addresses, take care about potential aliasing
		 * before reading the page on the kernel side.
		 */
		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);
#endif

		/*
		 * When a sequential read accesses a page several times,
		 * only mark it as accessed the first time.
		 */
		if (prev_index != index || offset != prev_offset)
			mark_page_accessed(page);
		prev_index = index;

		/*
		 * Ok, we have the page, and it's up-to-date, so
		 * now we can copy it to user space...
		 *
		 * The actor routine returns how many bytes were actually used..
		 * NOTE! This may not be the same as how much of a user buffer
		 * we filled up (we may be padding etc), so we can only update
		 * "pos" here (the actor routine has to update the user buffer
		 * pointers and the remaining count).
		 */
		ret = actor(desc, page, offset, nr);
		offset += ret;
		index += offset >> PAGE_CACHE_SHIFT;
		offset &= ~PAGE_CACHE_MASK;
		prev_offset = offset;

		page_cache_release(page);
		if (ret == nr && desc->count)
			continue;
		goto out;

page_not_up_to_date:
		/* Get exclusive access to the page ... */
		error = lock_page_killable(page);
		if (unlikely(error))
			goto readpage_error;

		printk(KERN_ALERT "%s:%d page_not_up_to_date\n",
			__FUNCTION__,__LINE__); 

page_not_up_to_date_locked:
		/* Did it get truncated before we got the lock? */
		if (!page->mapping) {

			printk(KERN_ALERT "%s:%d page_not_up_to_date_locked\n",
				__FUNCTION__,__LINE__); 

			unlock_page(page);
			page_cache_release(page);
			continue;
		}


		/* Did somebody else fill it already? */
		if (PageUptodate(page)) {

			printk(KERN_ALERT "%s:%d Failed\n",
				__FUNCTION__,__LINE__); 

			unlock_page(page);
			goto page_ok;
		}

		printk(KERN_ALERT "%s:%d page_not_up_to_date_locked\n",
			__FUNCTION__,__LINE__); 

readpage:
		/*
		 * A previous I/O error may have been due to temporary
		 * failures, eg. multipath errors.
		 * PG_error will be set again if readpage fails.
		 */
		ClearPageError(page);
		/* Start the actual read. The read will unlock the page. */
		//error = mapping->a_ops->readpage(filp, page);
		error = crfss_readpage(filp, page);

		printk(KERN_ALERT "DEBUG: On %s:%d \n",__FUNCTION__,__LINE__);

		if (unlikely(error)) {
			if (error == AOP_TRUNCATED_PAGE) {
				page_cache_release(page);
				goto find_page;
			}
			goto readpage_error;
		}

		printk(KERN_ALERT "%s:%d readpage\n",
			__FUNCTION__,__LINE__); 
#if 0
		if (!PageUptodate(page)) {
			error = lock_page_killable(page);
			if (unlikely(error))
				goto readpage_error;
			if (!PageUptodate(page)) {
				if (page->mapping == NULL) {
					/*
					 * invalidate_mapping_pages got it
					 */
					unlock_page(page);
					page_cache_release(page);
					goto find_page;
				}
				unlock_page(page);
				shrink_readahead_size_eio(filp, ra);
				error = -EIO;
				goto readpage_error;
			}
			unlock_page(page);
		}
#endif
		goto page_ok;

readpage_error:
		/* UHHUH! A synchronous read error occurred. Report it */
		desc->error = error;
		page_cache_release(page);
		goto out;

no_cached_page:
		/*
		 * Ok, it wasn't cached, so we need to create a new
		 * page..
		 */
//#ifdef HETEROMEM
#if 0
		if(current && current->heteroflag == PF_HETEROMEM){
			printk(KERN_ALERT "mm/filemap.c, Hetero before page cache alloc 8888\n"); 
            //page = getnvpage(NULL);
        }
        //if(!page)
#endif	
		page = page_cache_alloc_cold(mapping);
		if (!page) {
			desc->error = -ENOMEM;
			goto out;
		}
		error = add_to_page_cache_lru(page, mapping,
						index, GFP_KERNEL);
		if (error) {
			page_cache_release(page);
			if (error == -EEXIST)
				goto find_page;
			desc->error = error;
			goto out;
		}

#ifdef _DEVFS_VERBOSE
		printk(KERN_ALERT "@@@@@ xpfn: %d\n", page_to_pfn(page)); 
#endif

		goto readpage;
	}

out:
	ra->prev_pos = prev_index;
	ra->prev_pos <<= PAGE_CACHE_SHIFT;
	ra->prev_pos |= prev_offset;

#ifdef _DEVFS_SCALABILITY_DBG
	printk(KERN_ALERT "index = %lu, offset = %llu\n", index, offset);
#endif
	*ppos = ((loff_t)index << PAGE_CACHE_SHIFT) + offset;
	file_accessed(filp);
}



/************************** DevFS IO handler****************************************/
int crfss_readpage(struct file *file, struct page *page)
{
	//dump_stack();
        clear_highpage(page);
        flush_dcache_page(page);
        SetPageUptodate(page);
        unlock_page(page);
        return 0;
}

int crfss_segment_checks(const struct iovec *iov,
                        unsigned long *nr_segs, size_t *count, int access_flags)
{
        unsigned long   seg;
        size_t cnt = 0;
        for (seg = 0; seg < *nr_segs; seg++) {
                const struct iovec *iv = &iov[seg];
                /*
                 * If any segment has a negative length, or the cumulative
                 * length ever wraps negative then return -EINVAL.
                 */
                cnt += iv->iov_len;
                if (unlikely((ssize_t)(cnt|iv->iov_len) < 0))
                        return -EINVAL;
                if (access_ok(access_flags, iv->iov_base, iv->iov_len))
                        continue;
                if (seg == 0)
                        return -EFAULT;
                *nr_segs = seg;
                cnt -= iv->iov_len;     /* This segment is no good */
                break;
        }
        *count = cnt;
        return 0;
}
//EXPORT_SYMBOL(generic_segment_checks);


/**
 * generic_file_aio_read - generic filesystem read routine
 * @iocb:	kernel I/O control block
 * @iov:	io vector request
 * @nr_segs:	number of segments in the iovec
 * @pos:	current file position
 *
 * This is the "read()" routine for all filesystems
 * that can use the page cache directly.
 */
ssize_t
crfss_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	ssize_t retval;
	unsigned long seg = 0;
	size_t count;
	loff_t *ppos = &iocb->ki_pos;

	count = 0;
	retval = crfss_segment_checks(iov, &nr_segs, &count, VERIFY_WRITE);
	if (retval)
		return retval;

	count = retval;
	for (seg = 0; seg < nr_segs; seg++) {
		read_descriptor_t desc;
		loff_t offset = 0;

		/*
		 * If we did a short DIO read we need to skip the section of the
		 * iov that we've already read data into.
		 */
		if (count) {
			if (count > iov[seg].iov_len) {
				count -= iov[seg].iov_len;
				continue;
			}
			offset = count;
			count = 0;
		}

		desc.written = 0;
		desc.arg.buf = iov[seg].iov_base + offset;
		desc.count = iov[seg].iov_len - offset;
		if (desc.count == 0)
			continue;
		desc.error = 0;
		crfss_generic_file_read(filp, ppos, &desc, file_read_actor);
		retval += desc.written;

#ifdef _DEVFS_VERBOSE
		printk(KERN_ALERT "*****: desc.written:%d desc.count:%d %s:%d \n",desc.written,desc.count,__FUNCTION__,__LINE__);
#endif

		if (desc.error) {
			retval = retval ?: desc.error;
			break;
		}
		if (desc.count > 0)
			break;
	}
	return retval;
}
EXPORT_SYMBOL(crfss_file_aio_read);




ssize_t crfss_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
        struct iovec iov = { .iov_base = buf, .iov_len = len };
        struct kiocb kiocb;
        ssize_t ret;

        init_sync_kiocb(&kiocb, filp);
        kiocb.ki_pos = *ppos;
        //kiocb.ki_left = len;
        //kiocb.ki_nbytes = len;

#if !defined(_DEVFS_CHECK)
        for (;;) {
#endif
                ret = crfss_file_aio_read(&kiocb, &iov, 1, kiocb.ki_pos);

                if (ret != -EIOCBRETRY)
#if !defined(_DEVFS_CHECK)
                        break;
                //wait_on_retry_sync_kiocb(&kiocb);
        }
#endif
        *ppos = kiocb.ki_pos;
        return ret;
}

EXPORT_SYMBOL(crfss_sync_read);



ssize_t crfss_read(struct file *file, char __user *buf, 
	size_t count, loff_t *pos)
{
        ssize_t ret;

#if 1 //!defined(_DEVFS_CHECK)
        if (!(file->f_mode & FMODE_READ)) {
                printk(KERN_ALERT "!(file->f_mode & FMODE_READ) \n");
                return -EBADF;
        }
        if (unlikely(!access_ok(VERIFY_WRITE, buf, count))) {
                printk(KERN_ALERT "!access_ok(VERIFY_WRITE, buf, count)\n");
                return -EFAULT;
        }
        ret = rw_verify_area(READ, file, pos, count);
#else
		ret = count;
#endif
		ret = count;

        if (ret >= 0) {
                count = ret;
                ret = crfss_sync_read(file, buf, count, pos);

                if (ret > 0) {
                        fsnotify_access(file);
                        add_rchar(current, ret);
                } else {
			//printk("isize = %lu, count = %lu, pos = %llu", file->f_inode->i_size, count, *pos);
		}
                inc_syscr(current);

        }else {
                printk(KERN_ALERT "rw_verify_area failed, ret = %d\n", (int)ret);
        }
        return ret;
}
EXPORT_SYMBOL(crfss_read);


ssize_t do_crfss_sync_write(struct file *filp, 
	const char __user *buf, size_t len, loff_t *ppos)
{
        struct iovec iov = { .iov_base = (void __user *)buf, .iov_len = len };
        struct kiocb kiocb;
        ssize_t ret;

        init_sync_kiocb(&kiocb, filp);
        kiocb.ki_pos = *ppos;
        //kiocb.ki_left = len;
        //kiocb.ki_nbytes = len;


#ifdef _DEVFS_DEBUG_RDWR
	printk(KERN_ALERT "%s:%d pos %llu\n",__FUNCTION__,__LINE__, *ppos);
#endif

#if !defined(_DEVFS_CHECK)
        for (;;) {
                ret = filp->f_op->aio_write(&kiocb, &iov, 1, kiocb.ki_pos);
                if (ret != -EIOCBRETRY)
                        break;
                wait_on_retry_sync_kiocb(&kiocb);
        }

        if (-EIOCBQUEUED == ret)
                ret = wait_on_sync_kiocb(&kiocb);

#else
        ret = crfss_file_aio_write(&kiocb, &iov, 1, kiocb.ki_pos);
#endif

        *ppos = kiocb.ki_pos;
        return ret;
}
EXPORT_SYMBOL(do_crfss_sync_write);



crfss_transaction_t *crfss_begin_inode_trans(struct super_block *sb, 
		struct inode *inode, struct crfss_inode *ei,  
		unsigned long max_logentries, u64 nvaddr) {

	struct crfs_inode *pi = NULL;
	crfs_transaction_t *trans;
	
	/*pi = devfss_get_inode(sb, inode);

	printk(KERN_ALERT "pi = %llx | %s:%d\n", pi, __FUNCTION__, __LINE__);

	if(!pi) {
		printk(KERN_ALERT "pi = %llx | %s:%d\n", pi, __FUNCTION__, __LINE__);
		BUG_ON(pi);
	}

	trans = crfss_new_transaction(ei, MAX_INODE_LENTRIES + max_logentries);
	if (!trans) {
		BUG_ON(!trans);
		return NULL;
	}
	if (crfss_add_logentry(ei, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA)) {
		BUG_ON(trans);
		return NULL;
	}*/

	pi = crfs_get_inode(sb, inode->i_ino);

	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES + max_logentries);
	if (IS_ERR(trans)) {
		BUG_ON(!trans);
		return NULL;
	}
	/* Add address of data block in NVM to the log entry */
	crfs_add_logentry(sb, trans, (void*)nvaddr, MAX_DATA_PER_LENTRY, LE_DATA);
	crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	return (crfss_transaction_t*)trans;
}



ssize_t crfss_kernel_write(struct file *file, const char *buf, 
		size_t count, loff_t *pos, u64 nvaddr)
{

#if 1 //!defined(_DEVFS_CHECK)
        mm_segment_t old_fs;
#endif
        const char __user *p;
        ssize_t ret;

#if defined(_DEVFS_JOURN)
	crfss_transaction_t *trans;
	struct super_block *sb;
	struct inode *inode;
	struct crfss_inode *ei;
	unsigned long start_blk, num_blocks, max_logentries;
	size_t offset;
	loff_t ppos;
	struct dentry *dentry;
#endif


#if 1 //!defined(_DEVFS_CHECK)
	/* file_operations.aio_write is deprecated.*/
        // if (!file->f_op || (!file->f_op->write && !file->f_op->aio_write)) {
	if (!file->f_op || (!file->f_op->write)) {
                printk(KERN_ALERT "__kernel_write no handler\n");
                return -EINVAL;
        }
        old_fs = get_fs();
        set_fs(get_ds());
#endif


#if defined(_DEVFS_JOURN)
	inode = file->f_inode;
	ei = DEVFS_I(inode);
	sb = inode->i_sb;
	dentry = file->f_dentry;

	if(!ei->isjourn) {
		printk(KERN_ALERT "name %s inode %d cache not initialized "
			"ei ptr %lu \n", dentry->d_name.name, inode->i_ino, 
			(unsigned long)ei);
		BUG_ON(!ei->isjourn);
	}

	/*if(unlikely(ei->cachep_init != CACHEP_INIT)){
		printk(KERN_ALERT "name %s inode %d cache not initialized \n", 
				inode->i_ino, dentry->d_name.name);
		BUG_ON(!ei->cachep_init);
	}*/

	ppos = *pos;
	offset = ppos & (sb->s_blocksize - 1);
	num_blocks = ((count + offset - 1) >> sb->s_blocksize_bits) + 1;

	max_logentries = num_blocks / MAX_PTRS_PER_LENTRY + 2;
	if (max_logentries > MAX_METABLOCK_LENTRIES)
        	 max_logentries = MAX_METABLOCK_LENTRIES;

#if defined(_DEVFS_NOVA_BASED)
	trans = crfss_begin_inode_trans(sb, inode, ei, max_logentries, nvaddr);
	if(!trans) {
		printk(KERN_ALERT "__kernel_write %d \n", ei->isjourn);
		BUG_ON(!trans);	
	}
#endif

#endif

        p = (__force const char __user *)buf;
        if (count > MAX_RW_COUNT)
                count =  MAX_RW_COUNT;

#if !defined(_DEVFS_CHECK)
        if (file->f_op->write)
                ret = file->f_op->write(file, p, count, pos);
        else
	        ret = do_sync_write(file, p, count, pos);
#else

#ifdef _DEVFS_DEBUG_RDWR
	printk(KERN_ALERT "%s:%d pos %llu\n",__FUNCTION__,__LINE__, *pos);
#endif

	ret = do_crfss_sync_write(file, p, count, pos);
#endif

        /*if(ret != count) {
                printk(KERN_ALERT "__kernel_write not complete \n");
        }*/

#if 1// !defined(_DEVFS_CHECK)
        set_fs(old_fs);
#endif

        if (ret > 0) {
                fsnotify_modify(file);
                add_wchar(current, ret);
        }

        inc_syscw(current);

#if defined(_DEVFS_JOURN)
	//crfss_commit_transaction(ei, trans);
	crfs_commit_transaction(sb, trans);
#endif

        return ret;
}
EXPORT_SYMBOL(crfss_kernel_write);



/*
 * devfs fname hash
 */
unsigned int crfss_str_hash_linux(const char *str, unsigned int length)
{
        unsigned long hash = 0;
        unsigned char c;

        while (length--) {
                c = *str++;
                hash = (hash + (c << 4) + (c >> 4)) * 11;
        }
        return hash;
}


unsigned int crfss_str_hash(const char *s, unsigned int len)
{
	return crfss_str_hash_linux(s, len);
}
EXPORT_SYMBOL(crfss_str_hash);


struct crfss_inotree *crfss_inode_list_create(void)
{
        struct crfss_inotree *inotree;

        inotree = kzalloc(sizeof(*inotree), GFP_KERNEL);
        if (!inotree)
                return ERR_PTR(-ENOMEM);

	/*initialize devfs inode tree*/
	inotree->initalize = 1;

        inotree->inode_list = RB_ROOT;
        mutex_init(&inotree->lock);

        return inotree;
}
EXPORT_SYMBOL(crfss_inode_list_create);



/*add inodes to rbtree node */
//int insert_inode_rbtree(struct crfss_inotree *inotree, struct crfss_inode *ei){
int insert_inode_rbtree(struct rb_root *root, struct crfss_inode *ei){

	struct rb_node **new = &(root->rb_node), *parent = NULL;
	//struct rb_node **new = &inotree->inode_list.rb_node, *parent = NULL;
	int retval = 0;
	unsigned long i_ino = 0;

	crfss_dbgv("************START****************\n");

	if(!new) {
	    printk(KERN_ALERT "%s:%d rbtree root NULL \n",__FUNCTION__, __LINE__);
	    retval = -EFAULT;
	    goto err_insert_inode;
	}

	if(!ei) {
		printk("%s:%d Inode is NULL \n", __FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_insert_inode;
	}

	crfss_dbgv("%s:%d Before ei->vfs_inode->i_ino\n", 
			__FUNCTION__,__LINE__);

#if !defined(_DEVFS_INODE_OFFLOAD)
	i_ino = ei->vfs_inode.i_ino;
#else
	i_ino = ei->vfs_inode->i_ino;
#endif
	if(!i_ino) {
	    printk(KERN_ALERT "%s:%d Invalid inode number\n", 
		__FUNCTION__,__LINE__);
	    retval = -EFAULT;
	    goto err_insert_inode;	   	
	} 

	crfss_dbgv("%s:%d After ei->vfs_inode->i_ino\n", 
			__FUNCTION__,__LINE__);

	/* Figure out where to put new node */
	while (*new) {

		struct crfss_inode *this = rb_entry(*new, struct crfss_inode, rbnode);
		parent = *new;

		if(!this ) {
			printk("%s:%d Failed \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_insert_inode;
		}
#if !defined(_DEVFS_INODE_OFFLOAD)
		crfss_dbgv("%s:%d After struct crfss_inode *this %lu\n", 
			__FUNCTION__,__LINE__, this->vfs_inode.i_ino);
#else
		crfss_dbgv("%s:%d After struct crfss_inode *this %lu\n", 
			__FUNCTION__,__LINE__, this->vfs_inode->i_ino);
#endif


#if !defined(_DEVFS_INODE_OFFLOAD)
		if (i_ino < this->vfs_inode.i_ino) {
			new = &((*new)->rb_left);
		}else if (i_ino > this->vfs_inode.i_ino){
			new = &((*new)->rb_right);
		}else{
			goto err_insert_inode;
		}

#else
		if (i_ino < this->vfs_inode->i_ino) {
			new = &((*new)->rb_left);
		}else if (i_ino > this->vfs_inode->i_ino){
			new = &((*new)->rb_right);
		}else{
			goto err_insert_inode;
		}
#endif

                crfss_dbgv("%s:%d After rb_entry insert\n", __FUNCTION__,__LINE__);
	}

	if(!new) 
		goto err_insert_inode;
	
	/* Add new node and rebalance tree. */
	rb_link_node(&ei->rbnode, parent, new);
	rb_insert_color(&ei->rbnode, root);

#if defined(_DEVFS_DEBUG)
	printk("%s:%d success\n",  __FUNCTION__,__LINE__);
#endif
	crfss_dbgv("************END****************\n");

//insert_ok:
	return 0;

err_insert_inode:
	crfss_dbgv("************END ERROR************\n");

	return -1;
}
EXPORT_SYMBOL(insert_inode_rbtree);


void del_inode_rbtree(struct crfss_inotree *inotree, struct crfss_inode *ei)
{
        rb_erase(&ei->rbnode, &inotree->inode_list);
}
EXPORT_SYMBOL(del_inode_rbtree);


/*find crfss_inode from the inode number */
struct crfss_inode *find_crfss_inode(struct crfss_inotree *inotree, 
				unsigned long i_ino) {

	struct rb_node **new = &inotree->inode_list.rb_node, *parent = NULL;

        /* Figure out where to put new node */
        while (*new) {

              struct crfss_inode *this = rb_entry(*new, struct crfss_inode, rbnode);
              parent = *new;

              if(!this ) {
                      printk("%s:%d Failed \n",__FUNCTION__,__LINE__);
                      goto err_find_inode;
              }

#if !defined(_DEVFS_INODE_OFFLOAD)
              if (i_ino < this->vfs_inode.i_ino) {
                      new = &((*new)->rb_left);
              }else if (i_ino > this->vfs_inode.i_ino){
                      new = &((*new)->rb_right);
              }else{
                      return this;
              }
#else
              if (i_ino < this->vfs_inode->i_ino) {
                      new = &((*new)->rb_left);
              }else if (i_ino > this->vfs_inode->i_ino){
                      new = &((*new)->rb_right);
              }else{
                      return this;
              }
#endif
        }
err_find_inode:
        printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
        return NULL;
}
EXPORT_SYMBOL(find_crfss_inode);


static inline int build_open_flags(int flags, umode_t mode, struct open_flags *op)
{
	int lookup_flags = 0;
	int acc_mode;

	if (flags & O_CREAT)
		op->mode = (mode & S_IALLUGO) | S_IFREG;
	else
		op->mode = 0;

	/* Must never be set by userspace */
	flags &= ~FMODE_NONOTIFY & ~O_CLOEXEC;

	/*
	 * O_SYNC is implemented as __O_SYNC|O_DSYNC.  As many places only
	 * check for O_DSYNC if the need any syncing at all we enforce it's
	 * always set instead of having to deal with possibly weird behaviour
	 * for malicious applications setting only __O_SYNC.
	 */
	if (flags & __O_SYNC)
		flags |= O_DSYNC;

	/*
	 * If we have O_PATH in the open flag. Then we
	 * cannot have anything other than the below set of flags
	 */
	if (flags & O_PATH) {
		flags &= O_DIRECTORY | O_NOFOLLOW | O_PATH;
		acc_mode = 0;
	} else {
		acc_mode = MAY_OPEN | ACC_MODE(flags);
	}

	op->open_flag = flags;

	/* O_TRUNC implies we need access checks for write permissions */
	if (flags & O_TRUNC)
		acc_mode |= MAY_WRITE;

	/* Allow the LSM permission hook to distinguish append
	   access from general write access. */
	if (flags & O_APPEND)
		acc_mode |= MAY_APPEND;

	op->acc_mode = acc_mode;

	op->intent = flags & O_PATH ? 0 : LOOKUP_OPEN;

	if (flags & O_CREAT) {
		op->intent |= LOOKUP_CREATE;
		if (flags & O_EXCL)
			op->intent |= LOOKUP_EXCL;
	}

	if (flags & O_DIRECTORY)
		lookup_flags |= LOOKUP_DIRECTORY;
	if (!(flags & O_NOFOLLOW))
		lookup_flags |= LOOKUP_FOLLOW;
	return lookup_flags;
}


void crfss_final_putname(struct filename *name)
{
	if (name->separate) {
		__putname(name->name);
		kfree(name);
	} else {
		__putname(name);
	}
}

#define EMBEDDED_NAME_MAX	(PATH_MAX - sizeof(struct filename))

static struct filename *
crfss_getname_flags(const char __user *filename, int flags, int *empty, char *s_kname)
{
	struct filename *result, *err;
	int len;
	long max;
	char *kname;

	result = audit_reusename(filename);
	if (result)
		return result;

	result = __getname();
	if (unlikely(!result)) {
		printk("__getname failed \n");
		return ERR_PTR(-ENOMEM);
	}

	/*
	 * First, try to embed the struct filename inside the names_cache
	 * allocation
	 */
	kname = (char *)result + sizeof(*result);
	result->name = kname;
	result->separate = false;
	max = EMBEDDED_NAME_MAX;

recopy:
	//len = strncpy_from_user(kname, filename, max);
	memcpy(kname, s_kname, max);
	len = strlen(kname);
	if (unlikely(len < 0)) {
		printk("strncpy_from_user failed\n");
		err = ERR_PTR(len);
		goto error;
	}

	/*
	 * Uh-oh. We have a name that's approaching PATH_MAX. Allocate a
	 * separate struct filename so we can dedicate the entire
	 * names_cache allocation for the pathname, and re-do the copy from
	 * userland.
	 */
	if (len == EMBEDDED_NAME_MAX && max == EMBEDDED_NAME_MAX) {
		kname = (char *)result;

		result = kzalloc(sizeof(*result), GFP_KERNEL);
		if (!result) {
			err = ERR_PTR(-ENOMEM);
			result = (struct filename *)kname;
			goto error;
		}
		result->name = kname;
		result->separate = true;
		max = PATH_MAX;
		goto recopy;
	}

	result->refcnt = 1;
	/* The empty path is special. */
	if (unlikely(!len)) {
		if (empty)
			*empty = 1;
		err = ERR_PTR(-ENOENT);
		printk("getname unlikely(!len) failed\n");	
		if (!(flags & LOOKUP_EMPTY))
			goto error;
	}

	err = ERR_PTR(-ENAMETOOLONG);
	if (unlikely(len >= PATH_MAX))
		goto error;

	result->uptr = filename;
	audit_getname(result);
	return result;

error:
	crfss_final_putname(result);
	return err;
}

struct filename *
crfss_getname(const char __user * filename, char *s_kname)
{
	return crfss_getname_flags(filename, 0, NULL, s_kname);
}


static struct inode *load_inode(struct inode *inode) 
{
	BUG_ON(!inode);

	if(inode && inode->crfss_host) {	

		crfss_dbgv("INODE in host memory \n");

		inode = inode_ld_from_host(inode->i_sb, inode);
		BUG_ON(!inode);					
		BUG_ON(inode->crfss_host);
	}
	return inode;
}


//long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode)
struct file *crfss_create_file(char *filename, int flags, umode_t mode, int *fd1)
{
    struct open_flags op;
    int lookup = build_open_flags(flags, mode, &op);
    struct filename *tmp;
    char __user *fname;
    struct file *f = NULL;
    int dfd = AT_FDCWD;
    int fd = -1;
#if defined(_DEVFS_INODE_OFFLOAD)
    struct inode *inode = NULL;
    struct crfss_inode *ei = NULL;
    struct dentry *dentry = NULL;
#endif

    fname = (__force char __user *)filename;
    tmp = crfss_getname(fname, filename);
    fd = PTR_ERR(tmp);

    if (!IS_ERR(tmp)) {
    //printk(KERN_ALERT "pid = %d, name = %s before\n", current->pid, filename);
	fd = get_unused_fd_flags(flags);
    //printk(KERN_ALERT "pid = %d, name = %s after, fd = %d\n", current->pid, filename, fd);
        if (fd >= 0) {
		    f = do_filp_open_new(dfd, tmp, &op, lookup);
		    if (IS_ERR(f)) {
				put_unused_fd(fd);
				fd = PTR_ERR(f);
				printk(KERN_ALERT "Failed fd = %d |%s:%d \n",
					fd, __FUNCTION__,__LINE__);
				f = NULL;
	            } else {

			fsnotify_open(f);
			fd_install(fd, f);

#if defined(_DEVFS_INODE_OFFLOAD)
			if(f->f_inode && !f->f_inode->crfss_host)
				goto skip_inooff;

			ei = DEVFS_I(f->f_inode);

			BUG_ON(ei);

			//BUGGY. More than one dentry can exist
			// for an inode
			ei->dentry = f->f_dentry;

			inode = load_inode(f->f_inode);
			if (!inode) {
				printk("%s:%d Failed to load inode "
					"from host memory \n",
					__FUNCTION__,__LINE__);
				BUG_ON(!inode);
			} else {

				printk("%s:%d Success to load inode "
					"from host memory \n",
					__FUNCTION__,__LINE__);
			}
			f->f_inode = inode;
			dentry = f->f_dentry;
			dentry->d_inode = inode;
skip_inooff:
				BUG_ON(!f->f_inode);
#endif
			}
        } else {
			printk(KERN_ALERT "Failed %s:%d fname %s, fd = %d\n",
				__FUNCTION__,__LINE__, filename, fd);
		}
        putname(tmp);
    } else {
		printk(KERN_ALERT "Failed %s:%d fname %s\n",
		__FUNCTION__,__LINE__, filename);
	}
	*fd1 = fd;
    return f;
}


#if 0
struct file *crfss_create_file(const char *filename, int flags, umode_t mode, int *fd) 
{	
    struct file *fp = NULL; 

#ifdef _DEVFS_DEBUG_ENT
    printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);	
#endif

    *fd = get_unused_fd_flags(flags);
    if (*fd >= 0) {

	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(get_ds());

        fp = filp_open(filename, flags, mode);

	set_fs(oldfs);

        if(!fp) {
            printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);	
	    return NULL;	
        }
 
        fsnotify_open(fp);
	fd_install(*fd, fp);
    }
    return fp;
}
EXPORT_SYMBOL(crfss_create_file);
#endif

int crfss_permission(struct inode *inode, int mask){

#ifdef _DEVFS_DEBUG_ENT
	printk(KERN_ALERT "DEBUG: Calling %s:%d \n",__FUNCTION__,__LINE__);
#endif
	return generic_permission(inode, mask);
}

long do_crfss_unlink(const char __user *pathname) {
	int dfd = AT_FDCWD;
	return do_unlinkat(dfd, pathname);
}


/************************RAMFS copied code*****************/




/*static int
ext2_writepages(struct address_space *mapping, struct writeback_control *wbc)
{
        return mpage_writepages(mapping, wbc, ext2_get_block);
}*/


static size_t __iovec_copy_from_user_inatomic(char *vaddr,
			const struct iovec *iov, size_t base, size_t bytes)
{
	size_t copied = 0, left = 0;

	while (bytes) {
		char __user *buf = iov->iov_base + base;
		int copy = min(bytes, iov->iov_len - base);

		base = 0;
		left = __copy_from_user_inatomic(vaddr, buf, copy);
		copied += copy;
		bytes -= copy;
		vaddr += copy;
		iov++;

		if (unlikely(left))
			break;
	}
	return copied - left;
}



static const struct super_operations crfss_ops;
static const struct inode_operations crfss_dir_inode_operations;

ssize_t crfss_perform_write(struct file *file,
				struct iov_iter *i, loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
#if !defined(_DEVFS_CHECK)
	const struct address_space_operations *a_ops = mapping->a_ops;
#endif
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;

	char *kaddr;

#ifdef _DEVFS_DEBUG_RDWR
       printk(KERN_ALERT "%s:%d pos %llu\n",__FUNCTION__,__LINE__, pos);	
#endif

#if !defined(_DEVFS_CHECK)
	/*
	 * Copies from kernel address space cannot fail (NFSD is a big user).
	 */
	if (segment_eq(get_fs(), KERNEL_DS))
		flags |= AOP_FLAG_UNINTERRUPTIBLE;
#endif

	do {
		struct page *page = NULL;
		unsigned long offset = 0; /* Offset into pagecache page */
		unsigned long bytes = 0; /* Bytes to write to page */
		size_t copied = 0;	 /* Bytes copied from user */
		void *fsdata = NULL;

		offset = (pos & (PAGE_CACHE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_count(i));
again:
		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

#if defined(_DEVFS_CHECK)
		status = crfss_write_begin(file, mapping, pos, bytes, flags,&page, &fsdata);
#else
		status = a_ops->write_begin(file, mapping, pos, bytes, flags,
						&page, &fsdata);
#endif
		if (unlikely(status)) {
			printk(KERN_ALERT "Failed %s:%d copied %zu\n",
				 __FUNCTION__,__LINE__,copied);
			break;
		}

#if !defined(_DEVFS_CHECK)
                if (mapping_writably_mapped(mapping))
                        flush_dcache_page(page);
#endif

		pagefault_disable();

		//BUG_ON(!in_atomic());
		kaddr = kmap_atomic(page);
		if (likely(i->nr_segs == 1)) {
			int left;
			char __user *buf = i->iov->iov_base + i->iov_offset;
			left = __copy_from_user_inatomic(kaddr + offset, buf, bytes);
			copied = bytes - left;
		} else {
			copied = __iovec_copy_from_user_inatomic(kaddr + offset,
						i->iov, i->iov_offset, bytes);
		}

		kunmap_atomic(kaddr);

		pagefault_enable();


#if !defined(_DEVFS_CHECK)
		flush_dcache_page(page);
#endif
		mark_page_accessed(page);

#if defined(_DEVFS_CHECK)
		status = crfss_write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
#else
		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
#endif
		if (unlikely(status < 0)) {
			printk(KERN_ALERT "Failed %s:%d copied %zu\n",
				 __FUNCTION__,__LINE__,copied);
			break;
		}
		copied = status;

#if  !defined(_DEVFS_CHECK)
		cond_resched();
#endif

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;

#if !defined(_DEVFS_CHECK)
		balance_dirty_pages_ratelimited(mapping);

		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}
#endif
	} while (iov_iter_count(i));

#ifdef _DEVFS_DEBUG_RDWR
	printk(KERN_ALERT "%s:%d written %lu\n",__FUNCTION__,__LINE__, written);
	printk(KERN_ALERT "***************\n");
#endif

	return written ? written : status;
}
EXPORT_SYMBOL(crfss_perform_write);



ssize_t
crfss_file_buffered_write(struct kiocb *iocb, const struct iovec *iov,
                unsigned long nr_segs, loff_t pos, loff_t *ppos,
                size_t count, ssize_t written)
{
        struct file *file = iocb->ki_filp;
        ssize_t status;
        struct iov_iter i;

#ifdef _DEVFS_VERBOSE
		printk(KERN_ALERT "#### buf:%lx \n", iov->iov_base);
#endif

        //iov_iter_init(&i, iov, nr_segs, count, written);
		iov_iter_init(&i, 0, iov, nr_segs, count);

#ifdef _DEVFS_VERBOSE
		printk(KERN_ALERT "#### buf:%lx \n", iov->iov_base);
#endif

#ifdef _DEVFS_DEBUG
        printk(KERN_ALERT "%s:%d before generic_perform_write %lu\n",
                __FUNCTION__,__LINE__, written);
#endif


#ifdef _DEVFS_CHECK
        status = crfss_perform_write(file, &i, pos);
#else
        status = generic_perform_write(file, &i, pos);
#endif
        if (likely(status >= 0)) {
                written += status;
                *ppos = pos + status;
        }

#ifdef _DEVFS_DEBUG_RDWR
        printk(KERN_ALERT "%s:%d After generic_perform_write %lu\n",
                __FUNCTION__,__LINE__, written);
#endif

        return written ? written : status;
}
EXPORT_SYMBOL(crfss_file_buffered_write);


/*
 * Performs necessary checks before doing a write
 * @iov:        io vector request
 * @nr_segs:    number of segments in the iovec
 * @count:      number of bytes to write
 * @access_flags: type of access: %VERIFY_READ or %VERIFY_WRITE
 *
 * Adjust number of segments and amount of bytes to write (nr_segs should be
 * properly initialized first). Returns appropriate error code that caller
 * should return or zero in case that write should be allowed.
 */
#if 0
int crfss_segment_checks(const struct iovec *iov,
                        unsigned long *nr_segs, size_t *count, int access_flags)
{
        unsigned long   seg;
        size_t cnt = 0;
        for (seg = 0; seg < *nr_segs; seg++) {
                const struct iovec *iv = &iov[seg];

                /*
                 * If any segment has a negative length, or the cumulative
                 * length ever wraps negative then return -EINVAL.
                 */
                cnt += iv->iov_len;
                if (unlikely((ssize_t)(cnt|iv->iov_len) < 0))
                        return -EINVAL;
                if (access_ok(access_flags, iv->iov_base, iv->iov_len))
                        continue;
                if (seg == 0)
                        return -EFAULT;
                *nr_segs = seg;
                cnt -= iv->iov_len;     /* This segment is no good */
                break;
        }
        *count = cnt;

        /*printk(KERN_ALERT "%s:%d *count %zu %lu\n", 
		__FUNCTION__,__LINE__, *count, *nr_segs);*/
	
        return 0;
}
EXPORT_SYMBOL(crfss_segment_checks);
#endif


/**
 * __crfss_file_aio_write - write data to a file
 * @iocb:       IO state structure (file, offset, etc.)
 * @iov:        vector with data to write
 * @nr_segs:    number of segments in the vector
 * @ppos:       position where to write
 *
 * This function does all the work needed for actually writing data to a
 * file. It does all basic checks, removes SUID from the file, updates
 * modification times and calls proper subroutines depending on whether we
 * do direct IO or a standard buffered write.
 *
 * It expects i_mutex to be grabbed unless we work on a block device or similar
 * object which does not need locking at all.
 *
 * This function does *not* take care of syncing data in case of O_SYNC write.
 * A caller has to handle it. This is mainly due to the fact that we want to
 * avoid syncing under i_mutex.
 */
ssize_t __crfss_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                                 unsigned long nr_segs, loff_t *ppos)
{
        struct file *file = iocb->ki_filp;
        struct address_space * mapping = file->f_mapping;
        size_t ocount;          /* original count */
        size_t count;           /* after file limit checks */
#if !defined(_DEVFS_CHECK)
        struct inode    *inode = mapping->host;
#endif
        loff_t          pos;
        ssize_t         written;
        ssize_t         err = 0;

        ocount = 0;
#if 1 //!defined(_DEVFS_CHECK)
        err = crfss_segment_checks(iov, &nr_segs, &ocount, VERIFY_READ);
        if (err)
                return err;
#else
        nr_segs = 1;
        ocount = 4096;
#endif
        count = ocount;
        pos = *ppos;

#ifdef _DEVFS_DEBUG_RDWR
        printk(KERN_ALERT "%s:%d pos %llu\n",__FUNCTION__,__LINE__, pos);
#endif

        /* We can write back this queue in page reclaim */
        current->backing_dev_info = mapping->backing_dev_info;
        written = 0;

#if !defined(_DEVFS_CHECK)
        err = generic_write_checks(file, &pos, &count, S_ISBLK(inode->i_mode));
        if (err)
                goto out;

        if (count == 0)
                goto out;

        err = file_remove_suid(file);
        if (err)
                goto out;

        err = file_update_time(file);
        if (err)
                goto out;
#endif
	written = crfss_file_buffered_write(iocb, iov, nr_segs,
			pos, ppos, count, written);
	//written = PAGE_SIZE;

#ifdef _DEVFS_DEBUG_RDWR
	printk(KERN_ALERT "%s:%d write %lu\n",__FUNCTION__,__LINE__, written);
#endif
        current->backing_dev_info = NULL;
        return written ? written : err;
}
EXPORT_SYMBOL(__crfss_file_aio_write);


/**
 * generic_file_aio_write - write data to a file
 * @iocb:       IO state structure
 * @iov:        vector with data to write
 * @nr_segs:    number of segments in the vector
 * @pos:        position in file where to write
 *
 * This is a wrapper around __crfss_file_aio_write() to be used by most
 * filesystems. It takes care of syncing the file in case of O_SYNC file
 * and acquires i_mutex as needed.
 */
ssize_t crfss_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                unsigned long nr_segs, loff_t pos)
{
        struct file *file = iocb->ki_filp;
        struct inode *inode = file->f_mapping->host;
        ssize_t ret;

        BUG_ON(iocb->ki_pos != pos);

#ifdef _DEVFS_DEBUG_RDWR
        printk(KERN_ALERT "%s:%d pos %llu\n",__FUNCTION__,__LINE__, pos);
#endif

        sb_start_write(inode->i_sb);

#ifdef _USELOCK
        //mutex_lock(&inode->i_mutex);
       inode_crfss_lock(inode);
#endif

        ret = __crfss_file_aio_write(iocb, iov, nr_segs, &iocb->ki_pos);

#ifdef _USELOCK
        //mutex_unlock(&inode->i_mutex);
        inode_crfss_unlock(inode);
#endif

#if defined(_DEVFS_CHECK)
        if (ret > 0 || ret == -EIOCBQUEUED) {
                ssize_t err;

				// err = generic_write_sync(file, pos, ret);
				err = generic_write_sync(iocb, ret);


                if (err < 0 && ret > 0) {
#ifdef _DEVFS_DEBUG_RDWR
                		printk(KERN_ALERT "%s:%d pos %llu\n",__FUNCTION__,__LINE__, pos);
#endif
                        ret = err;
                }
        }
#endif
        sb_end_write(inode->i_sb);
        return ret;
}
EXPORT_SYMBOL(crfss_file_aio_write);


#if 1
int crfss_write_begin(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned flags,
                        struct page **pagep, void **fsdata)
{
        struct page *page;
        pgoff_t index;


	//printk(KERN_ALERT "************************\n");

	//printk(KERN_ALERT "%s:%d index %llu, pos %llu\n",
	//		__FUNCTION__,__LINE__, index, pos);


        index = pos >> PAGE_CACHE_SHIFT;

        page = grab_cache_page_write_begin(mapping, index, flags);
        if (!page)
                return -ENOMEM;

	if(page->nvdirty) {
		printk(KERN_ALERT "crfss_write_begin nvdirty set \n");	
	}else {
		//printk(KERN_ALERT "crfss_write_begin nvdirty not set \n");
	}

        *pagep = page;

        if (!PageUptodate(page) && (len != PAGE_CACHE_SIZE)) {
                unsigned from = pos & (PAGE_CACHE_SIZE - 1);

                zero_user_segments(page, 0, from, from + len, PAGE_CACHE_SIZE);
        }
        return 0;
}

#else
int crfss_write_begin(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned flags,
                        struct page **pagep, void **fsdata)
{
        struct page *page = NULL;
        pgoff_t index;
	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;
	crfss_transaction_t *trans;

        index = pos >> PAGE_SHIFT;

#if defined(_DEVFS_DEBUG_RDWR)
	printk(KERN_ALERT "begin %s:%d index %llu, pos %llu\n",
			__FUNCTION__,__LINE__, index, pos);
#endif

#if defined(_DEVFS_DEBUG)
	printk(KERN_ALERT "%s:%d pos %lu, index %lu \n",
		__FUNCTION__, __LINE__, pos, index);
#endif

	inode = file->f_inode;

	ei = DEVFS_I(inode);
	if(ei->isjourn) {
		trans = crfss_new_ino_trans_log(ei, inode);
		if(!trans) {
			printk(KERN_ALERT "%s:%d trans NULL \n",
				__FUNCTION__,__LINE__);
			 return -ENOMEM;
		}else {
			ei->trans = (crfss_transaction_t *)trans;		
		}
	}

#if defined(_DEVFS_DEBUG_RDWR)
	printk(KERN_ALERT "before %s:%d\n",__FUNCTION__,__LINE__);
#endif
	//page = crfss_get_cache_page(mapping, index, flags, ei, inode);
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;

#if defined(_DEVFS_DEBUG_RDWR)
	printk(KERN_ALERT "end %s:%d\n",__FUNCTION__,__LINE__);
#endif
        *pagep = page;
        if (!PageUptodate(page) && (len != PAGE_SIZE)) {
                unsigned from = pos & (PAGE_SIZE - 1);

                zero_user_segments(page, 0, from, from + len, PAGE_SIZE);
        }
#if defined(_DEVFS_DEBUG_RDWR)
	printk(KERN_ALERT "end %s:%d\n",__FUNCTION__,__LINE__);
#endif
        return 0;
}
#endif
EXPORT_SYMBOL(crfss_write_begin);



/*
 * NOTE: unlike i_size_read(), i_size_write() does need locking around it
 * (normally i_mutex), otherwise on 32bit/SMP an update of i_size_seqcount
 * can be lost, resulting in subsequent i_size_read() calls spinning forever.
 */
static inline void crfss_i_size_write(struct inode *inode, loff_t i_size)
{
        inode->i_size = i_size;
}

//////////////////In-memory inode/////////////////////////////////////////
inline void crfss_update_nlink(struct inode *inode, struct crfss_inode *ei)
{
        //crfss_memunlock_inode(inode->i_sb, ei);
        ei->i_nlink = cpu_to_le16(inode->i_nlink);
        //crfss_memlock_inode(inode->i_sb, ei);
}

inline void crfss_update_isize(struct inode *inode, struct crfss_inode *ei)
{
        //crfss_memunlock_inode(inode->i_sb, ei);
        ei->i_size = cpu_to_le64(inode->i_size);
        //crfss_memlock_inode(inode->i_sb, ei);
}

inline void crfss_update_time(struct inode *inode, struct crfss_inode *ei)
{
        //crfss_memunlock_inode(inode->i_sb, ei);
        ei->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
        ei->i_mtime = cpu_to_le32(inode->i_mtime.tv_sec);
        //crfss_memlock_inode(inode->i_sb, ei);
}

int crfss_inode_update(struct inode *inode, loff_t pos){

	struct timespec ts = CURRENT_TIME;
	struct crfss_inode *ei = DEVFS_I(inode);

	//crfss_memunlock_inode(inode->i_sb, ei);

	if(pos > ei->i_size) {
		ei->i_size = pos;
	}

	if(inode->i_size > ei->i_size){
		inode->i_size = pos;
	}

	ei->i_mtime = ei->i_ctime = cpu_to_le32(ts.tv_sec);

	//crfss_memlock_inode(inode->i_sb, ei);

	return 0;
}
//////////////////In-memory inode/////////////////////////////////////////

int crfss_set_page_dirty_nobuffers(struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct crfss_inode *ei = NULL;

        ei = DEVFS_I(inode);
	
        if (!TestSetPageDirty(page)) {
                struct address_space *mapping = page_mapping(page);
                struct address_space *mapping2;

                if (!mapping)
                        return 1;

#ifdef _USELOCK
                spin_lock_irq(&mapping->tree_lock);
#endif
                mapping2 = page_mapping(page);
                if (mapping2) { /* Race with truncate? */
                        BUG_ON(mapping2 != mapping);
                        WARN_ON_ONCE(!PagePrivate(page) && !PageUptodate(page));
                        account_page_dirtied(page, mapping);

#if !defined(_DEVFS_USE_INODE_PGTREE)
                        radix_tree_tag_set(&mapping->page_tree,
                                page_index(page), PAGECACHE_TAG_DIRTY);
#else
                        radix_tree_tag_set(&inode->page_tree,
                                page_index(page), PAGECACHE_TAG_DIRTY);
#endif
                }

#ifdef _USELOCK
                spin_unlock_irq(&mapping->tree_lock);
#endif
                if (mapping->host) {
                        /* !PageAnon && !swapper_space */
                        __mark_inode_dirty(mapping->host, I_DIRTY_PAGES);
                }
                return 1;
        }
        return 0;
}


int crfss_set_node_page_dirty(struct page *page)
{
        //struct address_space *mapping = page->mapping;

        SetPageUptodate(page);
        if (!PageDirty(page)) {
                crfss_set_page_dirty_nobuffers(page);
                SetPagePrivate(page);
                return 1;
        }
        return 0;
}
EXPORT_SYMBOL(crfss_set_node_page_dirty);

/**
 * crfss_write_end - .write_end helper for non-block-device FSes
 * @available: See .write_end of address_space_operations
 * @file:               "
 * @mapping:            "
 * @pos:                "
 * @len:                "
 * @copied:             "
 * @page:               "
 * @fsdata:             "
 *
 * crfss_write_end does the minimum needed for updating a page after writing is
 * done. It has the same API signature as the .write_end of
 * address_space_operations vector. So it can just be set onto .write_end for
 * FSes that don't need any other processing. i_mutex is assumed to be held.
 * Block based filesystems should use generic_write_end().
 * NOTE: Even though i_size might get updated by this function, mark_inode_dirty
 * is not called, so a filesystem that actually does store data in .write_inode
 * should extend on what's done here with a call to mark_inode_dirty() in the
 * has changed.
 */
#if 1
int crfss_write_end(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned copied,
                        struct page *page, void *fsdata)
{
        struct inode *inode = page->mapping->host;
        loff_t last_pos = pos + copied;

        /* zero the stale part of the page if we did a short copy */
        if (copied < len) {
                unsigned from = pos & (PAGE_CACHE_SIZE - 1);

                zero_user(page, from + copied, len - copied);
        }

        if (!PageUptodate(page))
                SetPageUptodate(page);
        /*
         * No need to use i_size_read() here, the i_size
         * cannot change under us because we hold the i_mutex.
         */
        if (last_pos > inode->i_size)
                i_size_write(inode, last_pos);

        set_page_dirty(page);
        unlock_page(page);
        page_cache_release(page);

	//printk(KERN_ALERT "%s:%d crfss_write_end \n", 
	//			__FUNCTION__,__LINE__);

        return copied;
}
#else
int crfss_write_end(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned copied,
                        struct page *page, void *fsdata)
{

#if !defined(_DEVFS_HETERO)
        struct inode *inode = page->mapping->host;
#else
	struct inode *inode = file->f_inode;
#endif

	//printk(KERN_ALERT "%s:%d\n",__FUNCTION__,__LINE__);

	crfss_transaction_t *trans = NULL;
        loff_t last_pos = pos + copied;
	struct crfss_inode *ei = NULL;

	ei = DEVFS_I(inode);

	if(ei->isjourn) {
		/*There can be only one active transaction per-file*/
		trans = (crfss_transaction_t *)ei->trans;
		if(!trans) {
			printk(KERN_ALERT "%s:%d trans NULL \n",
				__FUNCTION__,__LINE__);
			goto write_err;
		}
	}

        /* zero the stale part of the page if we did a short copy */
        if (copied < len) {
                unsigned from = pos & (PAGE_SIZE - 1);
                zero_user(page, from + copied, len - copied);
        }

        if (!PageUptodate(page))
                SetPageUptodate(page);

        /*
         * No need to use i_size_read() here, the i_size
         * cannot change under us because we hold the i_mutex.
         */
        if (last_pos > inode->i_size)
#if defined(_DEVFS_CHECK)
		i_size_write(inode, last_pos);
		crfss_update_isize(inode, ei);
#else
                i_size_write(inode, last_pos);
#endif
        crfss_update_time(inode, ei);

	if(ei->isjourn)
		crfss_commit_transaction(ei, trans);

        //set_page_dirty(page);
        //crfss_set_node_page_dirty(page);

#ifdef _USELOCK
        unlock_page(page);
#endif
        put_page(page);
        //printk(KERN_ALERT "%s:%d\n",__FUNCTION__,__LINE__);
        return copied;
write_err:
	return 0;
}
#endif
EXPORT_SYMBOL(crfss_write_end);


/*int crfss_release_file(struct inode *inode, struct file *filp)
{
        return 0;
}*/

static void crfss_get_inode_flags(struct inode *inode, struct devfss_inode *pi)
{
        unsigned int flags = inode->i_flags;
        unsigned int crfss_flags = le32_to_cpu(pi->i_flags);

        crfss_flags &= ~(FS_SYNC_FL | FS_APPEND_FL | FS_IMMUTABLE_FL |
                         FS_NOATIME_FL | FS_DIRSYNC_FL);
        if (flags & S_SYNC)
                crfss_flags |= FS_SYNC_FL;
        if (flags & S_APPEND)
                crfss_flags |= FS_APPEND_FL;
        if (flags & S_IMMUTABLE)
                crfss_flags |= FS_IMMUTABLE_FL;
        if (flags & S_NOATIME)
                crfss_flags |= FS_NOATIME_FL;
        if (flags & S_DIRSYNC)
                crfss_flags |= FS_DIRSYNC_FL;

        pi->i_flags = cpu_to_le32(crfss_flags);
}

#if defined(_DEVFS_DEBUG)
static void crfss_print_inode(struct inode *inode, struct devfss_inode *pi)
{

	printk("%s: i_mode %u, i_uid %u, i_gid %u "
		"i_links_count %u, i_size %u, i_blocks %llu "
		"i_atime %u, i_ctime %u, i_mtime %u, i_generation %u \n",
		__func__, pi->i_mode, pi->i_uid, pi->i_gid, pi->i_links_count,
		pi->i_size, pi->i_blocks, pi->i_atime, pi->i_ctime, 
		pi->i_ctime, pi->i_mtime, pi->i_generation);
}
#endif


#if 0
static int crfss_update_inode(struct inode *inode, struct devfss_inode *pi)
{

	if(!pi) {
		printk(KERN_ALERT "%s:%d pi NULL \n",__FUNCTION__,__LINE__);
		return -1;
	}

	if(!inode) {
		printk(KERN_ALERT "%s:%d inode NULL \n",__FUNCTION__,__LINE__);
		return -1;
	}


#if defined(_DEVFS_LOCKING)
        nova_memunlock_inode(inode->i_sb, pi);
#endif
        pi->i_mode = cpu_to_le16(inode->i_mode);
        pi->i_uid = cpu_to_le32(i_uid_read(inode));
        pi->i_gid = cpu_to_le32(i_gid_read(inode));
        pi->i_links_count = cpu_to_le16(inode->i_nlink);
        pi->i_size = cpu_to_le64(inode->i_size);
        pi->i_blocks = cpu_to_le64(inode->i_blocks);
        pi->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
        pi->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
        pi->i_mtime = cpu_to_le32(inode->i_mtime.tv_sec);
        pi->i_generation = cpu_to_le32(inode->i_generation);
	pi->nova_ino = inode->i_ino;
        crfss_get_inode_flags(inode, pi);

        if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
                pi->dev.rdev = cpu_to_le32(inode->i_rdev);

#if defined(_DEVFS_LOCKING)
        nova_memlock_inode(inode->i_sb, pi);
#endif
	return 0;

}
#endif



struct inode *crfss_get_inode(struct super_block *sb,
	const struct inode *dir, umode_t mode, dev_t dev)
{

#if defined(_DEVFS_DEBUG)
	void *func = &sb->s_op->alloc_inode;
	printk(KERN_ALERT "func: %pF at address: %p\n", func, func);
#endif

#if !defined(_DEVFS_NOVA_BASED)
	struct inode *inode = new_inode(sb);
	if (inode) 
#else
	struct inode *inode = NULL;
#endif
	{

#if defined(_DEVFS_NOVA_BASED)
		struct crfs_inode *pi = NULL;
#if 0
		u64 pi_addr = 0;

		//This should be renamed to crfss_inode_info
		struct crfss_inode *ei;

		u64 ino = devfss_new_crfss_inode(sb, &pi_addr);
		pi = (struct devfss_inode *)crfss_get_block(sb, pi_addr);
		if(!ino || !pi) {
			printk("%s: Failed invalid devfss inode no. or addr\n",
				 __func__);
			return NULL;
		}

		inode->i_ino = ino;
		inode_init_owner(inode, dir, mode);
#endif
		inode = crfs_new_inode(NULL, (struct inode *)dir, mode, NULL);
#else

		inode->i_ino = get_next_ino();
		inode_init_owner(inode, dir, mode);
#endif


#ifdef _DEVFS_DEBUG
		printk("%s: allocating inode %llu, pi %llu, sb %llu piaddr %llu\n",
			__func__, ino, (u64)pi, (u64)crfss_get_super(sb), pi_addr);
#endif

		inode->i_mapping->a_ops = &crfss_aops;
		mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
		mapping_set_unevictable(inode->i_mapping);
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;

		//inode->i_nlink = 0;

		switch (mode & S_IFMT) {
		default:
			init_special_inode(inode, mode, dev);
			break;
		case S_IFREG:
			inode->i_op = &crfss_file_inode_operations;
			inode->i_fop = &crfss_file_operations;
			//inode->i_op = &ramfs_file_inode_operations;
			//inode->i_fop = &ramfs_file_operations;
			break;
		case S_IFDIR:
			inode->i_op = &crfss_dir_inode_operations;
			inode->i_fop = &simple_dir_operations;
			//inode->i_op = &ramfs_dir_inode_operations;
			//inode->i_fop = &simple_dir_operations;
			/* directory inodes start off with i_nlink == 2 (for "." entry) */
			inc_nlink(inode);
			break;
		case S_IFLNK:
			inode->i_op = &page_symlink_inode_operations;
			//inode_nohighmem(inode);
			break;
		}

#if defined(_DEVFS_NOVA_BASED)
		/*if(crfss_update_inode(inode, pi)){
	        	printk("%s:%d crfss_update_inode failed \n",
				__FUNCTION__,__LINE__);
			return NULL;
		}
		ei = DEVFS_I(inode);
		if(!ei) 
	        	printk("%s:%d finding crfss_inode failed \n",
				__FUNCTION__,__LINE__);
		ei->pi_addr = pi_addr;*/
		//crfss_print_inode(inode, pi);
		crfs_update_inode(inode, pi);
#endif
	}
	return inode;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
/* SMP-safe */
int crfss_mknod(struct inode *dir, struct dentry *dentry, 
	umode_t mode, dev_t dev)
{
	int error = -ENOSPC;
	struct inode *inode = NULL;
	struct super_block *sb = dir->i_sb;
	struct crfss_sb_info *i_sb = DEVFS_SB(sb);
	struct crfss_inode *ei;


#if defined(_DEVFS_JOURN)
	crfss_transaction_t *trans = NULL;
#endif

	if(unlikely(!i_sb)) {
		printk(KERN_ALERT "%s:%d i_sb NULL \n",
			 __FUNCTION__,__LINE__);
		goto mknod_err;
	}

	if(!dentry) {
		printk(KERN_ALERT "%s:%d dentry NULL \n",
			 __FUNCTION__,__LINE__);
		goto mknod_err;
	}

	inode = crfss_get_inode(sb, dir, mode, dev);
	if(!inode) {	
		printk(KERN_ALERT "%s:%d crfss_get_inode NULL \n",
			 __FUNCTION__,__LINE__);
		goto mknod_err;		
	}
	inode->isdevfs = 1;

	if(unlikely(!inode)) {

		printk(KERN_ALERT "%s:%d inode NULL \n",
			 __FUNCTION__,__LINE__);
		goto mknod_err;
	}

	//crfss_new_inode(dir, inode, mode, &error);
#if defined(_DEVFS_JOURN)
	/* Check if directory specific transaction 
	* log is initialized If not, initialize
	*/
	eidir = DEVFS_I(dir);
	if(!eidir->isjourn) {
	      if(init_journal(dir)) {
		      printk(KERN_ALERT "%s:%d init_journal Failed \n",
				      __FUNCTION__,__LINE__);
		      goto mknod_err;
	      }
	      crfss_dbgv("%s: %s Dir journ init success i_ino %llu\n",
				__func__, dentry->d_name.name, dir->i_ino);
	}else {
	      crfss_dbgv("%s:%d dir journal already set to %d\n", 
				__FUNCTION__,__LINE__, eidir->isjourn);
	}

	trans = crfss_new_ino_trans_log(eidir, dir);
	if(!trans) {
		printk(KERN_ALERT "%s:%d Failed \n",
				__FUNCTION__, __LINE__);
		goto mknod_err;	
	}

	/* Initialize journaling for inode if already does not exist*/
	ei = DEVFS_I(inode);

	if(init_journal(inode)) {
	      goto mknod_err;
	}

        crfss_dbgv( "%s: %s mknode journal success i_ino %llu, isjourn %d, ei %lu\n",
		__func__, dentry->d_name.name, inode->i_ino, ei->isjourn, (unsigned long)ei);
#endif

	if (inode) {

#if defined(_DEVFS_NOVA_LOG)
		unsigned long ino = inode->i_ino;
		unsigned long tail = 0;

		ei = DEVFS_I(dir);

		if(!ei->dentry_tree_init) {
			INIT_RADIX_TREE(&ei->dentry_tree, GFP_ATOMIC);
			ei->dentry_tree_init = 1;
			crfss_dbgv("%s: Initializing inode %llu \n", __func__, ino);
		}
		error = crfss_add_dentry(dentry, ino, 0, 0, &tail);
		if (error){
			printk(KERN_ALERT "%s:%d crfss_add_dentry Failed\n", 
			__FUNCTION__,__LINE__);	
		}
#endif

#if defined(_DEVFS_DEBUG)
		printk(KERN_ALERT "%s:%d i_no %lu\n", 
			__FUNCTION__,__LINE__,inode->i_ino);	
#endif
		d_instantiate(dentry, inode);
		dget(dentry);	/* Extra count - pin the dentry in core */
		error = 0;
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		ei->dentry = dentry;
	}
#if defined(_DEVFS_JOURN)
	if(eidir->isjourn)
		crfss_commit_transaction(eidir, trans);

        crfss_dbgv("%s: %s mknode journal success i_ino %llu, isjourn %d\n",
		__func__, dentry->d_name.name, inode->i_ino, ei->isjourn);
#endif
	return 0;

mknod_err:
	return error;
}

#if 0
static int handle_create(const char *nodename, umode_t mode, struct device *dev)
{
        struct dentry *dentry;
        struct path path;
        int err;

        dentry = kern_path_create(AT_FDCWD, nodename, &path, 0);
        if (dentry == ERR_PTR(-ENOENT)) {
                create_path(nodename);
                dentry = kern_path_create(AT_FDCWD, nodename, &path, 0);
        }
        if (IS_ERR(dentry))
                return PTR_ERR(dentry);

        err = vfs_mknod(path.dentry->d_inode,
                        dentry, mode, dev->devt);
        if (!err) {
                struct iattr newattrs;

                /* fixup possibly umasked mode */
                newattrs.ia_mode = mode;
                newattrs.ia_valid = ATTR_MODE;
                mutex_lock(&dentry->d_inode->i_mutex);
                notify_change(dentry, &newattrs);
                mutex_unlock(&dentry->d_inode->i_mutex);

                /* mark as kernel-created inode */
                dentry->d_inode->i_private = &thread;
        }
        done_path_create(&path, dentry);
        return err;
}
#endif

int crfss_mkdir(struct inode * dir, struct dentry * dentry, 
	umode_t mode)
{

	int retval = crfss_mknod(dir, dentry, mode | S_IFDIR, 0);
	if (!retval)
		inc_nlink(dir);
#ifdef _DEVFS_DEBUG
	printk(KERN_ALERT "crfss_mkdir called \n");
#endif
	return retval;
}

int crfss_create(struct inode *dir, struct dentry *dentry, 
	umode_t mode, bool excl)
{
#ifdef _DEVFS_DEBUG
	printk(KERN_ALERT "crfss_create called \n");
#endif
	return crfss_mknod(dir, dentry, mode | S_IFREG, 0);
}

int crfss_symlink(struct inode * dir, struct dentry *dentry, 
	const char * symname)
{
	struct inode *inode;
	int error = -ENOSPC;

	crfss_dbgv("%s:%d Called \n",__FUNCTION__,__LINE__);

	inode = crfss_get_inode(dir->i_sb, dir, S_IFLNK|S_IRWXUGO, 0);
	if (inode) {
		int l = strlen(symname)+1;
		error = page_symlink(inode, symname, l);
		if (!error) {
			d_instantiate(dentry, inode);
			dget(dentry);
			dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		} else
			iput(inode);
	}
	return error;
}


int crfss_rmdir(struct inode *dir, struct dentry *dentry)
{
        printk(KERN_ALERT "%s:%d Called \n",
                __FUNCTION__,__LINE__);

        if (!simple_empty(dentry))
                return -ENOTEMPTY;

        drop_nlink(dentry->d_inode);
        simple_unlink(dir, dentry);
        drop_nlink(dir);
        return 0;
}

int crfss_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct crfss_inode *ei;

	ei = DEVFS_I(inode);
	//release_cache_pages(ei);

#if defined(_DEVFS_NOVA_LOG)
	crfss_dbgv("%s: %s\n", __func__, dentry->d_name.name);
	crfss_dbgv("%s: inode %lu, dir %lu\n", __func__,
				inode->i_ino, dir->i_ino);
	retval = crfss_remove_dentry(dentry, 0, 0, &pidir_tail);
	if (retval) {
		printk("%s: %s crfss_remove_dentry FAILED\n", 
				__func__, dentry->d_name.name);
	}
#endif
        inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
        drop_nlink(inode);
        dput(dentry);
        return 0;
}


/************ DevFS inode info allocation and release *****************/

static struct kmem_cache *crfss_inode_cachep;
static struct kmem_cache *crfss_vfsinode_cachep;

inline static struct inode *alloc_vfs_inode(void){

        return kmem_cache_alloc(crfss_vfsinode_cachep, GFP_KERNEL);
}

struct inode *crfss_alloc_inode(struct super_block *sb)
{
	struct crfss_inode *ei;

	ei = kmem_cache_alloc(crfss_inode_cachep, GFP_KERNEL);
	if (!ei) {
		printk(KERN_ALERT "%s:%d FAILED inode NULL \n",
				__FUNCTION__,__LINE__);
		goto err_alloc_inode;
	}

	ei->dentry_tree_init = 0;
	ei->isjourn = 0;
	ei->cachep_init = 0;

	rwlock_init(&ei->i_meta_lock);
	INIT_RADIX_TREE(&ei->sq_tree, GFP_ATOMIC);
	__SPIN_LOCK_UNLOCKED(ei->sq_tree_lock);
	ei->sq_tree_init = _DEVFS_INITIALIZE;

	ei->rd_nr = 0;

	crfss_dbgv("crfss_inode size %lu, vfs_inode size %lu \n",
			sizeof(struct crfss_inode), sizeof(struct inode));

#if defined(_DEVFS_INODE_OFFLOAD)
	ei->vfs_inode = alloc_vfs_inode();
	if (!ei->vfs_inode) {
		printk(KERN_ALERT "%s:%d FAILED inode NULL \n",__FUNCTION__,__LINE__);
		goto err_alloc_inode;

	}
	if(ei->vfs_inode){
		inode_init_once(ei->vfs_inode);
	}

	crfss_dbgv("%s:%d Success allocating inode\n",__FUNCTION__,__LINE__);

	ei->vfs_inode->crfss_host = 0;
	//ei->i_block_alloc_info = NULL;
	ei->vfs_inode->i_version = 1;
	return ei->vfs_inode;
#else
	ei->vfs_inode.i_version = 1;
	return &ei->vfs_inode;
#endif

err_alloc_inode:
	printk(KERN_ALERT "%s:%d FAILED inode NULL \n",__FUNCTION__,__LINE__);
	return NULL;

}


#if defined(_DEVFS_INODE_OFFLOAD)
/*
* Function to take an DevFS memory and offload it to host memory
* BUG Do offload only for file structures
*/
struct inode *inode_offld_to_host(struct super_block *sb, 
		struct inode *inode, struct dentry *dentry)
{
        struct crfss_inode *ei = DEVFS_I(inode);
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	struct inode *h_inode;

        if(!sbi->inodeoffsz) {
                printk(KERN_ALERT "%s:%d FAILED Invalid inodeoffsz \n",
                         __FUNCTION__,__LINE__);
                goto skip_ioffload;
        }

        if(!sbi->i_host_addr) {
                printk(KERN_ALERT "%s:%d FAILED Invalid i_host_addr \n",
                         __FUNCTION__,__LINE__);
                goto skip_ioffload;
        }

	if(!inode) {
		printk(KERN_ALERT "%s:%d FAILED Invalid inode \n",
                         __FUNCTION__,__LINE__);
		goto skip_ioffload;
	}

#if defined(_DEVFS_SLAB_ALLOC)	
	 BUG_ON(!sbi->i_host_slab);
         h_inode = (struct inode *)crfss_slab_alloc(sbi, sbi->i_host_slab, 
			&sbi->i_host_addr, &sbi->i_host_off);
#else
        h_inode = (struct inode *)(sbi->i_host_addr + sbi->i_host_off);
        sbi->i_host_off += sizeof(struct inode);
#endif

        if(!h_inode) {
                printk(KERN_ALERT "%s:%d FAILED invalid h_inode\n",
                __FUNCTION__,__LINE__);
		goto skip_ioffload;
        }

	memcpy(h_inode, inode, sizeof(struct inode));

        ei->vfs_inode = h_inode;
        if (!ei->vfs_inode) {
		printk("%s:%d FAILED inode NULL \n",__FUNCTION__,__LINE__);
                goto skip_ioffload;

	}
	dentry->d_inode = h_inode;

	/* Set host inode operation structures */
	h_inode->i_op = &crfss_file_inode_operations;
	h_inode->i_fop = &crfss_file_operations;
	h_inode->crfss_host = 1;

        //DEVFS BUGGY
        //Now we are only calling init_once on address space.
        //Calling on the entire inode_init_once will internally
        //perform memset and will clear the entire memory. As
        // a result, all the inode pointers copied will be reset
        if(h_inode){
              //inode_init_once(ei->vfs_inode);
              address_space_init_once(&h_inode->i_data);
              BUG_ON(!list_empty(&h_inode->i_data.private_list));
        }

	kmem_cache_free(crfss_vfsinode_cachep, inode);

	//if(ei->vfs_inode){
          //      inode_init_once(ei->vfs_inode);
        //}
	//crfss_dbgv("%s:%d Alloc inode offload path success\n",__FUNCTION__,__LINE__);
        //ei->i_block_alloc_info = NULL;
        //ei->vfs_inode->i_version = 1;
	return ei->vfs_inode;

skip_ioffload:
	printk("%s:%d offload failed \n",__FUNCTION__,__LINE__);
	return NULL;
}


/*
* Function to take an DevFS memory and offload it to host memory
* BUG Do offload only for file structures
*/
struct inode *inode_ld_from_host(struct super_block *sb, 
		struct inode *h_inode)
{
        struct crfss_inode *ei = DEVFS_I(h_inode);
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	struct inode *inode = NULL;

        if(!ei) {
                printk(KERN_ALERT "%s:%d FAILED crfss_inode \n",
                         __FUNCTION__,__LINE__);
                goto skip_ino_load;
        }
	
        if(!sbi->i_host_addr) {
                printk(KERN_ALERT "%s:%d FAILED Invalid i_host_addr \n",
                         __FUNCTION__,__LINE__);
                goto skip_ino_load;
        }

	if(!h_inode) {
		printk(KERN_ALERT "%s:%d FAILED Invalid inode \n",
                         __FUNCTION__,__LINE__);
		goto skip_ino_load;
	}

	//Allocate a new inode
        inode = alloc_vfs_inode();
        if (!inode) {
		printk("%s:%d FAILED inode NULL \n",__FUNCTION__,__LINE__);
                goto skip_ino_load;
	}
	
	memcpy(inode, h_inode, sizeof(*h_inode));

        ei->vfs_inode = inode;
        if (!ei->vfs_inode) {
		printk("%s:%d FAILED inode NULL \n",__FUNCTION__,__LINE__);
                goto skip_ino_load;
	}

	//mark the host memory inode invalid
	h_inode->crfss_flags = HOST_INODE_DELETED;

	//Set the operations structures.
	inode->i_op = &crfss_file_inode_operations;
	inode->i_fop = &crfss_file_operations;

	//DEVFS BUGGY
	//Now we are only calling init_once on address space.
	//Calling on the entire inode_init_once will internally 
	//perform memset and will clear the entire memory. As 
	// a result, all the inode pointers copied will be reset
	if(ei->vfs_inode){

  	      //inode_init_once(ei->vfs_inode);
	      address_space_init_once(&inode->i_data);		
	
	      BUG_ON(!list_empty(&inode->i_data.private_list));
        }

	//Disable the inode crfss_host flag now that we will use 
	//device memory
	ei->vfs_inode->crfss_host = 0;

	crfss_dbgv("%s:%d Alloc inode offload path success\n",__FUNCTION__,__LINE__);
        //ei->i_block_alloc_info = NULL;
        //ei->vfs_inode->i_version = 1;
	return ei->vfs_inode;

skip_ino_load:
	printk("%s:%d inode load failed \n",__FUNCTION__,__LINE__);
	return NULL;
}
#endif


static void crfss_i_callback(struct rcu_head *head)
{
	struct crfss_inode *ei;
        struct inode *inode = container_of(head, struct inode, i_rcu);

	if(!inode){
		printk("%s:%d inode NULL \n",__FUNCTION__,__LINE__);
		return;
	}

	ei = DEVFS_I(inode);
	if(!inode){
		printk("%s:%d crfss_inode NULL \n",__FUNCTION__,__LINE__);
		return;
	}

#if defined(_DEVFS_INODE_OFFLOAD)
	//Even if the inode was in host memory it should have 
	//been loaded back to the device memory by now
	//So, ideally freeing should not cause any concern
        kmem_cache_free(crfss_vfsinode_cachep, inode);
#endif
        kmem_cache_free(crfss_inode_cachep, ei);


#if  defined(_DEVFS_DEBUG)
	printk("%s:%d Finish \n",__FUNCTION__,__LINE__);
#endif
}

static void crfss_delete_inodes(struct inode *inode)
{
	struct crfss_inode *ei;

	if(!inode){
		printk("%s:%d inode NULL \n",__FUNCTION__,__LINE__);
		return;
	}

	ei = DEVFS_I(inode);
	if(!inode){
		printk("%s:%d crfss_inode NULL \n",__FUNCTION__,__LINE__);
		return;
	}

#if defined(_DEVFS_JOURN)
        if(ei->isjourn)
                free_journal(inode);
#endif

	//Even if the inode was in host memory it should have 
	//been loaded back to the device memory by now
	//So, ideally freeing should not cause any concern
        kmem_cache_free(crfss_vfsinode_cachep, inode);
        kmem_cache_free(crfss_inode_cachep, ei);

#if  defined(_DEVFS_DEBUG)
	printk("%s:%d Finish \n",__FUNCTION__,__LINE__);
#endif
}


unsigned int release_cache_pages(struct crfss_inode *ei, struct inode *inode)
{
	void **slot;
	unsigned int ret = 0;
	struct radix_tree_iter iter;
	//unsigned int nr_pages;
	pgoff_t start = 0, index = 0;
	struct address_space *mapping = inode->i_mapping;

	rcu_read_lock();

restart:

#if defined(_DEVFS_USE_INODE_PGTREE)
	radix_tree_for_each_slot(slot, &inode->page_tree, &iter, start) {
#else
	radix_tree_for_each_slot(slot, &mapping->page_tree, &iter, start) {
#endif
		struct page *page;
repeat:
		page = radix_tree_deref_slot(slot);
		if (unlikely(!page))
			continue;

		index = page_index(page);	
	
		printk("page index %lu \n", index);

		if(index) {
#if defined(_DEVFS_USE_INODE_PGTREE)
			if(radix_tree_delete(&inode->page_tree, index) == NULL) 
#else
			if(radix_tree_delete(&mapping->page_tree, index) == NULL)
#endif
				printk("radix page index delete failed %lu \n", index);
		}

		printk("page not null %d\n", ret);

		if (radix_tree_exception(page)) {
			if (radix_tree_deref_retry(page))
				goto restart;
			/*
			 * Otherwise, we must be storing a swap entry
			 * here as an exceptional entry: so return it
			 * without attempting to raise page count.
			 */
			goto export;
		}
		if (!page_cache_get_speculative(page))
			goto repeat;

		/* Has the page moved? */
		if (unlikely(page != *slot)) {
			page_cache_release(page);
			goto repeat;
		}
export:
		//if (++ret == nr_pages)
		//	break;
		ret++;
		page_cache_release(page);
	}

	rcu_read_unlock();

	printk("release_cache_pages exiting %d\n", ret);
	return ret;
}

void crfss_evict_inode(struct inode *inode)
{

#if defined(_DEVFS_INODE_OFFLOAD)
	struct dentry *dentry = NULL;

        crfss_dbg("%s:%d Begin \n",__FUNCTION__,__LINE__);

	//Reload the inode from host to the device 
	//memory if the inode was located in the host
	if (inode->crfss_host) {

		//dentry = d_find_any_alias(inode);
		//To many use of BUG_ON?
		//BUG_ON(!dentry);
		inode = load_inode(inode);

		if (!inode) {
			printk("%s:%d Failed to load inode "
				"from host memory \n",
				__FUNCTION__,__LINE__);
			BUG_ON(!inode);
		}
		inode->i_state = I_FREEING | I_CLEAR;
		BUG_ON(inode->i_state != (I_FREEING | I_CLEAR));
	}
#endif
	crfss_dbg("%s:%d \n",__FUNCTION__,__LINE__);
        //clear_inode(inode);

	crfss_dbg("%s:%d \n",__FUNCTION__,__LINE__);
        //kfree(inode->i_private);

	crfss_dbg("%s:%d End \n",__FUNCTION__,__LINE__);
}


void crfss_destroy_inode(struct inode *inode)
{
	struct crfss_inode *ei;
	//struct dentry *dentry = NULL;

	printk(KERN_ALERT "crfss_destroy_inode getting called \n");

	ei = DEVFS_I(inode);

	if(!inode){
		printk("%s:%d inode NULL \n",__FUNCTION__,__LINE__);
		return;
	}
	release_cache_pages(ei, inode);

#if defined(_DEVFS_BITMAP)
        if(crfss_free_inode(inode)){
                printk("%s:%d bitmap free inode failed \n",
                        __FUNCTION__,__LINE__);
        }
#endif

//PMFS CAUTION COMMENT
#if defined(_DEVFS_NOVA_BASED)
	//devfss_free_inode(inode);
#endif
	crfss_dbgv("Calling crfss_destroy_inode \n");

#if defined(_DEVFS_INODE_OFFLOAD)
	//Reload the inode from host to the device 
	//memory if the inode was located in the host
	if (inode->crfss_host) {

	        //printk(KERN_ALERT "%s:%d dentry %lu\n", 
                  //      (unsigned long)dentry, __FUNCTION__,__LINE__);
		inode = load_inode(inode);
		if (!inode) {
			printk("%s:%d Failed to load inode "
				"from host memory \n",
				__FUNCTION__,__LINE__);
			BUG_ON(!inode);
		}
		//inode->i_state = I_FREEING | I_CLEAR;
	}
	crfss_delete_inodes(inode);
#else
        call_rcu(&inode->i_rcu, crfss_i_callback);
#endif
	crfss_dbgv("Finished crfss_destroy_inode \n");
}

/************ DevFS inode info allocation and release *****************/

struct crfss_mount_opts {
	umode_t mode;
};

enum {
	Opt_mode,
	Opt_err
};

static const match_table_t tokens = {
	{Opt_mode, "mode=%o"},
	{Opt_err, NULL}
};

struct crfss_fs_info {
	struct crfss_mount_opts mount_opts;
};

static int crfss_parse_options(char *data, struct crfss_mount_opts *opts)
{
	substring_t args[MAX_OPT_ARGS];
	int option;
	int token;
	char *p;

	opts->mode = RAMFS_DEFAULT_MODE;

	while ((p = strsep(&data, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_mode:
			if (match_octal(&args[0], &option))
				return -EINVAL;
			opts->mode = option & S_IALLUGO;
			break;
		/*
		 * We might like to report bad mount options here;
		 * but traditionally devfs has ignored all mount options,
		 * and as it is used as a !CONFIG_SHMEM simple substitute
		 * for tmpfs, better continue to ignore other mount options.
		 */
		}
	}

	return 0;
}

#if defined(_DEVFS_BITMAP)
struct buffer_head *crfss_sb_bread(struct super_block *s, unsigned long block)
{
	struct buffer_head *bh = NULL;
	bh = kzalloc(sizeof(struct buffer_head), GFP_KERNEL);	
	if(!bh){
		return NULL;	
	}
	bh->b_data = kzalloc(PAGE_SIZE, GFP_KERNEL);	
	if(!bh->b_data){
		return NULL;	
	}
	return bh;
}

static int crfss_fill_super_extend(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh;
	struct buffer_head **map;
	struct crfss_super_block *ms;
	unsigned long i, block;
	struct inode *root_inode;
	struct crfss_sb_info *sbi;
	int ret = -EINVAL;

	sbi = kzalloc(sizeof(struct crfss_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;

	s->s_fs_info = sbi;



#if 0

	if (!sb_set_blocksize(s, PAGE_SIZE))
		goto out_bad_hblock;

	if (!(bh = crfss_sb_bread(s, 1)))
		goto out_bad_sb;

	ms = (struct crfss_super_block *) bh->b_data;
	sbi->s_ms = ms;
	sbi->s_sbh = bh;
	//sbi->s_mount_state = ms->s_state;
	sbi->s_ninodes = ms->s_ninodes;
	sbi->s_nzones = ms->s_nzones;
	sbi->s_imap_blocks = ms->s_imap_blocks;
	sbi->s_zmap_blocks = ms->s_zmap_blocks;
	sbi->s_firstdatazone = ms->s_firstdatazone;
	//sbi->s_log_zone_size = ms->s_log_zone_size;
	sbi->s_max_size = ms->s_max_size;
	s->s_magic = ms->s_magic;
#endif

	sbi->s_ninodes = 1;
	sbi->s_nzones = 1;
	sbi->s_imap_blocks = 10;
	sbi->s_zmap_blocks = 10;
	sbi->s_firstdatazone = 30;


	//sbi->s_version = MINIX_V1;
	sbi->s_dirsize = 32;
	sbi->s_namelen = 30;
	//s->s_max_links = MINIX_LINK_MAX;

	/*
	 * Allocate the buffer map to keep the superblock small.
	 */
	if (sbi->s_imap_blocks == 0 || sbi->s_zmap_blocks == 0)
		goto out_illegal_sb;
	i = (sbi->s_imap_blocks + sbi->s_zmap_blocks) * sizeof(bh);
	map = kzalloc(i, GFP_KERNEL);
	if (!map)
		goto out_no_map;

	sbi->s_imap = &map[0];
	sbi->s_zmap = &map[sbi->s_imap_blocks];


	block=2;
	for (i=0 ; i < sbi->s_imap_blocks ; i++) {
		if (!(sbi->s_imap[i]=crfss_sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}

	for (i=0 ; i < sbi->s_zmap_blocks ; i++) {
		if (!(sbi->s_zmap[i]=crfss_sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}

	crfss_set_bit(0,sbi->s_imap[0]->b_data);
	crfss_set_bit(0,sbi->s_zmap[0]->b_data);

	/* Apparently minix can create filesystems that allocate more blocks for
	 * the bitmaps than needed.  We simply ignore that, but verify it didn't
	 * create one with not enough blocks and bail out if so.
	 */
	block = crfss_blocks_needed(sbi->s_ninodes, s->s_blocksize);
	if (sbi->s_imap_blocks < block) {
		printk("MINIX-fs: file system does not have enough "
				"imap blocks allocated.  Refusing to mount\n");
		goto out_no_bitmap;
	}

	block = crfss_blocks_needed(
			(sbi->s_nzones - (sbi->s_firstdatazone + 1)),
			s->s_blocksize);
	if (sbi->s_zmap_blocks < block) {
		printk("MINIX-fs: file system does not have enough "
				"zmap blocks allocated.  Refusing to mount.\n");
		goto out_no_bitmap;
	}

	crfss_set_sb_bit(s);


#if 0
	/* set up enough so that it can read an inode */
	s->s_op = &minix_sops;
	root_inode = minix_iget(s, MINIX_ROOT_INO);
	if (IS_ERR(root_inode)) {
		ret = PTR_ERR(root_inode);
		goto out_no_root;
	}

	ret = -ENOMEM;
	s->s_root = d_make_root(root_inode);
	if (!s->s_root)
		goto out_no_root;

	if (!(s->s_flags & MS_RDONLY)) {
		if (sbi->s_version != MINIX_V3) /* s_state is now out from V3 sb */
			ms->s_state &= ~MINIX_VALID_FS;
		mark_buffer_dirty(bh);
	}
	if (!(sbi->s_mount_state & MINIX_VALID_FS))
		printk("MINIX-fs: mounting unchecked file system, "
			"running fsck is recommended\n");
	else if (sbi->s_mount_state & MINIX_ERROR_FS)
		printk("MINIX-fs: mounting file system with errors, "
			"running fsck is recommended\n");
#endif

	return 0;

out_no_root:
	if (!silent)
		printk("MINIX-fs: get root inode failed\n");
	goto out_freemap;

out_no_bitmap:
	printk("MINIX-fs: bad superblock or unable to read bitmaps\n");
out_freemap:
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	kfree(sbi->s_imap);
	goto out_release;

out_no_map:
	ret = -ENOMEM;
	if (!silent)
		printk("MINIX-fs: can't allocate map\n");
	goto out_release;

out_illegal_sb:
	if (!silent)
		printk("MINIX-fs: bad superblock\n");
	goto out_release;

out_no_fs:
	if (!silent)
		printk("VFS: Can't find a Minix filesystem V1 | V2 | V3 "
		       "on device %s.\n", s->s_id);
out_release:
	brelse(bh);
	goto out;

out_bad_hblock:
	printk("MINIX-fs: blocksize too small for device\n");
	goto out;

out_bad_sb:
	printk("MINIX-fs: unable to read superblock\n");
out:
	s->s_fs_info = NULL;
	kfree(sbi);
	return ret;
}


#if 0
static int minix_write_inode(struct inode *inode, struct writeback_control *wbc)
{
        int err = 0;
        struct buffer_head *bh;

        bh = V1_minix_update_inode(inode);

        if (!bh)
                return -EIO;
        if (wbc->sync_mode == WB_SYNC_ALL && buffer_dirty(bh)) {
                sync_dirty_buffer(bh);
                if (buffer_req(bh) && !buffer_uptodate(bh)) {
                        printk("IO error syncing minix inode [%s:%08lx]\n",
                                inode->i_sb->s_id, inode->i_ino);
                        err = -EIO;
                }
        }
        brelse (bh);
        return err;
}
#endif


#endif

int crfss_fill_super(struct super_block *sb, void *data, int silent)
{
	return crfs_fill_super(sb, data, silent);
}

#if 0
int crfss_fill_super(struct super_block *sb, void *data, int silent)
{
	struct crfss_fs_info *fsi;
	struct inode *inode;
	int err;

#if 1 //defined(DEVFS_DAX)
       //unsigned long blocksize;
       unsigned long initsize = 0;
       //struct devfss_inode *root_pi;
#endif

#ifdef _DEVFS_NOVA_BASED
	struct devfss_inode *devffs_inode = NULL;
	size_t size = 17179869184;
	struct crfss_sb_info *sbi;
	int i = 0;
	struct inode_map *inode_map;

	sbi = kzalloc(sizeof(struct crfss_sb_info), GFP_KERNEL);
	if (!sbi) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);		
		return -ENOMEM;
	}
	sb->s_fs_info = sbi;

        sbi->phys_addr = get_phys_addr(&data);
        if (sbi->phys_addr == (phys_addr_t)ULLONG_MAX) {
               printk(KERN_ALERT "%s:%d Failed Invalid physaddr \n",
			__FUNCTION__,__LINE__);
	}
	//TODO: Perform all size checking. Everything missing	
	crfss_parse_options2(data, sbi, 0);

	if(sbi->vmmode)
		size = 2097152;	
	
	if(!sbi->initsize)	
	        sbi->initsize = size;

        initsize = sbi->initsize;
#endif

#if 0
       /* Init a new pmfs instance */
        if (initsize) {
                root_pi = crfss_init(sb, initsize);

                if (IS_ERR(root_pi)) {
                      printk(KERN_ALERT "crfss_init failed \n");
                }

        }
#endif



#ifdef _DEVFS_DEBUG
	printk(KERN_ALERT "crfss_fill_super called \n");
#endif
	save_mount_options(sb, data);

	fsi = kzalloc(sizeof(struct crfss_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = crfss_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_SIZE;
	sb->s_blocksize_bits	= PAGE_SHIFT;
	sb->s_magic		= DEVFS_MAGIC;
	sb->s_op		= &crfss_ops;
	sb->s_time_gran		= 1;

#if defined(_DEVFS_BITMAP)
	crfss_fill_super_extend(sb, NULL, 0);
#endif


#if defined(_DEVFS_NOVA_BASED)
	sb->s_fs_info = sbi;

	crfss_set_default_opts(sbi);

        if (crfss_alloc_block_free_lists(sb)) {
		return -ENOMEM;
        }

        sbi->inode_maps = kzalloc(sbi->cpus * sizeof(struct inode_map),
                                        GFP_KERNEL);
        if (!sbi->inode_maps) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);		
		return -ENOMEM;
        }

        for (i = 0; i < sbi->cpus; i++) {
                inode_map = &sbi->inode_maps[i];
                mutex_init(&inode_map->inode_table_mutex);
                inode_map->inode_inuse_tree = RB_ROOT;
        }

        mutex_init(&sbi->s_lock);

        sbi->zeroed_page = kzalloc(PAGE_SIZE, GFP_KERNEL);
        if (!sbi->zeroed_page) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);
                return -ENOMEM;
        }

	devffs_inode = devfss_init(sb, size);
	if(!devffs_inode) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);		
		return -ENOMEM;
	}

        printk(KERN_ALERT "checking physical address 0x%016llx "
		"for devfs size %zu sbi->virt_addr %lu, sbi->d_host_addr %lu "
		"VM Mode set? %d sbi->inodeoffsz %lu\n", 
		(u64)sbi->phys_addr, size, (unsigned long)sbi->virt_addr,
		(unsigned long)sbi->d_host_addr, sbi->vmmode, sbi->inodeoffsz);
#endif
	inode = crfss_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}
#endif

#if 0
int crfss_fill_super(struct super_block *sb, void *data, int silent)
{
	struct crfss_fs_info *fsi;
	struct inode *inode;
	int err;

#if 1 //defined(DEVFS_DAX)
       unsigned long blocksize, initsize = 0;
       struct devfss_inode *root_pi;
#endif

#ifdef _DEVFS_NOVA_BASED
	struct devfss_inode *devffs_inode = NULL;
#if defined(_DEVFS_VM)
	size_t size = 2097152;	
#else
	size_t size = 17179869184;
#endif
	struct crfss_sb_info *sbi;
	int i = 0;
	struct inode_map *inode_map;

	sbi = kzalloc(sizeof(struct crfss_sb_info), GFP_KERNEL);
	if (!sbi) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);		
		return -ENOMEM;
	}
	sb->s_fs_info = sbi;

        sbi->phys_addr = get_phys_addr(&data);
        if (sbi->phys_addr == (phys_addr_t)ULLONG_MAX) {
               printk(KERN_ALERT "%s:%d Failed Invalid physaddr \n",
			__FUNCTION__,__LINE__);
	}

       printk(KERN_ALERT "checking physical address 0x%016llx for devfs size %zu\n",
                  (u64)sbi->phys_addr, size);

        sbi->initsize = size;
        initsize = sbi->initsize;
#endif

#if 0
       /* Init a new pmfs instance */
        if (initsize) {
                root_pi = crfss_init(sb, initsize);

                if (IS_ERR(root_pi)) {
                      printk(KERN_ALERT "crfss_init failed \n");
                }

        }
#endif



#ifdef _DEVFS_DEBUG
	printk(KERN_ALERT "crfss_fill_super called \n");
#endif
	save_mount_options(sb, data);

	fsi = kzalloc(sizeof(struct crfss_fs_info), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if (!fsi)
		return -ENOMEM;

	err = crfss_parse_options(data, &fsi->mount_opts);
	if (err)
		return err;

	sb->s_maxbytes		= MAX_LFS_FILESIZE;
	sb->s_blocksize		= PAGE_SIZE;
	sb->s_blocksize_bits	= PAGE_SHIFT;
	sb->s_magic		= DEVFS_MAGIC;
	sb->s_op		= &crfss_ops;
	sb->s_time_gran		= 1;

#if defined(_DEVFS_BITMAP)
	crfss_fill_super_extend(sb, NULL, 0);
#endif


#if defined(_DEVFS_NOVA_BASED)
	sb->s_fs_info = sbi;

	crfss_set_default_opts(sbi);

        if (crfss_alloc_block_free_lists(sb)) {
		return -ENOMEM;
        }

        sbi->inode_maps = kzalloc(sbi->cpus * sizeof(struct inode_map),
                                        GFP_KERNEL);
        if (!sbi->inode_maps) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);		
		return -ENOMEM;
        }

        for (i = 0; i < sbi->cpus; i++) {
                inode_map = &sbi->inode_maps[i];
                mutex_init(&inode_map->inode_table_mutex);
                inode_map->inode_inuse_tree = RB_ROOT;
        }

        mutex_init(&sbi->s_lock);

        sbi->zeroed_page = kzalloc(PAGE_SIZE, GFP_KERNEL);
        if (!sbi->zeroed_page) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);
                return -ENOMEM;
        }

	devffs_inode = devfss_init(sb, size);
	if(!devffs_inode) {
		printk("%s:%d Failed \n",__FUNCTION__,__LINE__);		
		return -ENOMEM;
	}
#endif

	inode = crfss_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
	sb->s_root = d_make_root(inode);
	if (!sb->s_root)
		return -ENOMEM;

	return 0;
}
#endif

struct dentry *crfss_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	printk(KERN_ALERT "crfss_mount called \n");
	//return mount_nodev(fs_type, flags, data, crfss_fill_super);
	return mount_nodev(fs_type, flags, data, crfs_fill_super);
}


static void crfss_kill_sb(struct super_block *sb)
{
	//kfree(sb->s_fs_info);
	printk(KERN_ALERT "crfss_kill_sb called \n");
	kill_litter_super(sb);
}


struct file_system_type crfss_fs_type = {
	.name		= "crfs",
	.mount		= crfss_mount,
	//.kill_sb	= crfss_kill_sb,
	.kill_sb	= kill_block_super,
        //.kill_sb        = kill_litter_super,
	.fs_flags	= FS_USERNS_MOUNT,
};


static void init_crfss_inode_once(void *foo)
{
        struct crfss_inode *ei = (struct crfss_inode *) foo;

        //sema_init(&ei->i_link_lock, 1);
        //sema_init(&ei->i_ext_lock, 1);
#if !defined(_DEVFS_INODE_OFFLOAD)
        inode_init_once(&ei->vfs_inode);
#else
	/*if(ei->vfs_inode){
		inode_init_once(ei->vfs_inode);
	}*/
#endif
}

/*TODO: Empty useless function*/
static void init_inode_once(void *foo)
{
   //struct inode *inode = (struct inode *) foo;
}


int init_crfss_inodecache(void)
{
        crfss_inode_cachep = kmem_cache_create("crfss_inode_cache",
                                             sizeof(struct crfss_inode),
                                             0, (SLAB_RECLAIM_ACCOUNT|
                                                SLAB_MEM_SPREAD),
                                             init_crfss_inode_once);
        if (crfss_inode_cachep == NULL) {
		printk(KERN_ALERT "FAILED %s:%d\n",__FUNCTION__,__LINE__);
                return -ENOMEM;
	}
        crfss_vfsinode_cachep = kmem_cache_create("crfss_vfsinode_cachep",
                                             sizeof(struct inode),
                                             0, (SLAB_RECLAIM_ACCOUNT|
                                                SLAB_MEM_SPREAD),
                                             init_inode_once);
        if (crfss_vfsinode_cachep == NULL) {
		printk(KERN_ALERT "FAILED %s:%d\n",__FUNCTION__,__LINE__);
                return -ENOMEM;
	}
        return 0;
}

void destroy_crfss_inodecache(void)
{
        /*
         * Make sure all delayed rcu free inodes are flushed before we
         * destroy cache.
         */
        rcu_barrier();
        kmem_cache_destroy(crfss_inode_cachep);
        kmem_cache_destroy(crfss_vfsinode_cachep);
}


#if 0 //def _DEVFS_NOVA_BASED

int crfss_init_inode_inuse_list(struct super_block *sb)
{
        struct crfss_sb_info *sbi = DEVFS_SB(sb);
        struct crfss_range_node *range_node;
        struct inode_map *inode_map;
        unsigned long range_high;
        int i;
        int ret;

        sbi->s_inodes_used_count = DEVFS_NORMAL_INODE_START;

        range_high = (DEVFS_NORMAL_INODE_START - 1) / sbi->cpus;
        if (DEVFS_NORMAL_INODE_START % sbi->cpus)
                range_high++;

        for (i = 0; i < sbi->cpus; i++) {
                inode_map = &sbi->inode_maps[i];
                range_node = crfss_alloc_inode_node(sb);
                if (range_node == NULL)
                        /* FIXME: free allocated memories */
                        return -ENOMEM;

                range_node->range_low = 0;
                range_node->range_high = range_high;
                ret = crfss_insert_inodetree(sbi, range_node, i);
                if (ret) {
                        printk("%s failed\n", __func__);
                        crfss_free_inode_node(sb, range_node);
                        return ret;
                }
                inode_map->num_range_node_inode = 1;
                inode_map->first_inode_range = (struct crfss_range_node *)range_node;
        }

        return 0;
}


int crfss_init_inode_table(struct super_block *sb)
{
        struct crfss_sb_info *sbi = DEVFS_SB(sb);
        struct inode_table *inode_table;
        struct devfss_inode *pi = crfss_get_inode_by_ino(sb, DEVFS_INODETABLE_INO);
        unsigned long blocknr;
        u64 block;
        int allocated;
        int i;

        pi->i_mode = 0;
        pi->i_uid = 0;
        pi->i_gid = 0;
        pi->i_links_count = cpu_to_le16(1);
        pi->i_flags = 0;
        pi->nova_ino = DEVFS_INODETABLE_INO;

	if(sbi->vmmode) {
	        pi->i_blk_type = DEVFS_BLOCK_TYPE_4K;
	}else {
	        pi->i_blk_type = DEVFS_BLOCK_TYPE_2M;
		//pi->i_blk_type = DEVFS_BLOCK_TYPE_4K;
	}

#if defined(_DEVFS_DEBUG)
	printk("%s:%d Enter\n", __FUNCTION__,__LINE__);	
#endif

        for (i = 0; i < sbi->cpus; i++) {

                inode_table = crfss_get_inode_table(sb, i);

                if (!inode_table) {
			printk("%s:%d Failed\n", __FUNCTION__,__LINE__);
                        return -EINVAL;
		}

                allocated = crfss_new_log_blocks(sb, pi, &blocknr, 1, 1);
                if (allocated != 1 || blocknr == 0) {
			printk("%s:%d Failed\n", __FUNCTION__,__LINE__);
                        return -ENOSPC;
		}

                block = crfss_get_block_off(sb, blocknr, pi->i_blk_type);
                inode_table->log_head = block;
	
#if defined(_DEVFS_DEBUG)	
		printk("%s:%d Block num %lu, Block off %llu\n", 
			__FUNCTION__,__LINE__, blocknr, block);
#endif

                crfss_flush_buffer(inode_table, CACHELINE_SIZE, 0);
        }
        DEVFS_PERSISTENT_BARRIER();
        return 0;
}


static int crfss_alloc_unused_inode(struct super_block *sb, int cpuid,
	unsigned long *ino)
{
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	struct inode_map *inode_map;
	struct crfss_range_node *i, *next_i;
	struct rb_node *temp, *next;
	unsigned long next_range_low;
	unsigned long new_ino;
	unsigned long MAX_INODE = 1UL << 31;

	inode_map = &sbi->inode_maps[cpuid];

	i = inode_map->first_inode_range;

#if defined(_DEVFS_DEBUG)
	printk(KERN_ALERT "range_low %d range_high %d \n", 
			i->range_low, i->range_high);
#endif

	DEVFS_ASSERT(i);

	temp = &i->node;
	next = rb_next(temp);

	if (!next) {
		next_i = NULL;
		next_range_low = MAX_INODE;
	} else {
		next_i = container_of(next, struct crfss_range_node, node);
		next_range_low = next_i->range_low;
	}

	new_ino = i->range_high + 1;

	if (next_i && new_ino == (next_range_low - 1)) {
		/* Fill the gap completely */
		i->range_high = next_i->range_high;
		rb_erase(&next_i->node, &inode_map->inode_inuse_tree);
		crfss_free_inode_node(sb, next_i);
		inode_map->num_range_node_inode--;
	} else if (new_ino < (next_range_low - 1)) {
		/* Aligns to left */
		i->range_high = new_ino;
	} else {
		printk("%s: ERROR: new ino %lu, next low %lu\n", __func__,
			new_ino, next_range_low);
		return -ENOSPC;
	}

	*ino = new_ino * sbi->cpus + cpuid;
	sbi->s_inodes_used_count++;
	inode_map->allocated++;

#if defined(_DEVFS_DEBUG)
	printk(KERN_ALERT "Alloc ino %lu\n", *ino);
#endif
	return 0;
}



static int crfss_free_inuse_inode(struct super_block *sb, unsigned long ino)
{
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	struct inode_map *inode_map;
	struct crfss_range_node *i = NULL;
	struct crfss_range_node *curr_node;
	int found = 0;
	int cpuid = ino % sbi->cpus;
	unsigned long internal_ino = ino / sbi->cpus;
	int ret = 0;

	inode_map = &sbi->inode_maps[cpuid];

	mutex_lock(&inode_map->inode_table_mutex);
	found = crfss_search_inodetree(sbi, ino, &i);

	if (!found) {

		printk("%s ERROR: ino %lu not found\n", __func__, ino);
		mutex_unlock(&inode_map->inode_table_mutex);
		return -EINVAL;
	}

	if ((internal_ino == i->range_low) && (internal_ino == i->range_high)) {
		/* fits entire node */
		rb_erase(&i->node, &inode_map->inode_inuse_tree);

		crfss_free_inode_node(sb, i);

		inode_map->num_range_node_inode--;
		goto block_found;
	}
	if ((internal_ino == i->range_low) && (internal_ino < i->range_high)) {
		/* Aligns left */
		i->range_low = internal_ino + 1;
		goto block_found;
	}
	if ((internal_ino > i->range_low) && (internal_ino == i->range_high)) {
		/* Aligns right */
		i->range_high = internal_ino - 1;
		goto block_found;
	}
	if ((internal_ino > i->range_low) && (internal_ino < i->range_high)) {

		/* Aligns somewhere in the middle */
		curr_node = crfss_alloc_inode_node(sb);

		DEVFS_ASSERT(curr_node);

		if (curr_node == NULL) {

			printk("curr_node == NULL %lu\n", internal_ino);
			/* returning without freeing the block */
			goto block_found;
		}

		curr_node->range_low = internal_ino + 1;
		curr_node->range_high = i->range_high;
		i->range_high = internal_ino - 1;

		ret = crfss_insert_inodetree(sbi, curr_node, cpuid);
		if (ret) {
			crfss_free_inode_node(sb, curr_node);
			goto err;
		}

#if defined(_DEVFS_DEBUG)
		printk("crfss_insert_inodetree %lu - %lu\n", 
				curr_node->range_low, curr_node->range_high);
#endif

		inode_map->num_range_node_inode++;
		goto block_found;
	}

err:
	printk("Unable to free inode %lu\n", ino);
	mutex_unlock(&inode_map->inode_table_mutex);
	return ret;

block_found:

#if defined(_DEVFS_DEBUG)
	printk(KERN_ALERT "Free inuse ino: %lu inode used count %u freed %u\n", 
		ino, sbi->s_inodes_used_count, inode_map->freed);
#endif
	sbi->s_inodes_used_count--;
	inode_map->freed++;
	mutex_unlock(&inode_map->inode_table_mutex);
	return ret;
}



static int crfss_free_contiguous_log_blocks(struct super_block *sb,
	struct devfss_inode *pi, u64 head)
{
	//struct crfss_inode_log_page *curr_page;
	unsigned long blocknr, start_blocknr = 0;
	u64 curr_block = head;
	u32 btype = pi->i_blk_type;
	int num_free = 0;
	int freed = 0;

	//printk("%s: curr_block %llu \n", __func__, curr_block);

	while (curr_block) {
		if (curr_block & INVALID_MASK) {
			printk("%s: ERROR: invalid block %llu\n",
					__func__, curr_block);
			break;
		}

		//curr_page = (struct crfss_inode_log_page *)crfss_get_block(sb,
		//					curr_block);

		blocknr = crfss_get_blocknr(sb, le64_to_cpu(curr_block),
				    btype);
		printk("%s: free page %llu\n", __func__, curr_block);

		//curr_block = curr_page->page_tail.next_page;

		if (start_blocknr == 0) {
			start_blocknr = blocknr;
			num_free = 1;
		} else {
			if (blocknr == start_blocknr + num_free) {
				num_free++;
			} else {
				/* A new start */
				crfss_free_log_blocks(sb, pi, start_blocknr,
							num_free);
				freed += num_free;
				start_blocknr = blocknr;
				num_free = 1;
			}
		}
	}
	if (start_blocknr) {
		crfss_free_log_blocks(sb, pi, start_blocknr, num_free);
		freed += num_free;
	}

	return freed;
}


void crfss_free_inode_log(struct super_block *sb, struct devfss_inode *pi)
{
        u64 curr_block;
        int freed = 0;

        if (pi->log_head == 0 || pi->log_tail == 0) {
		printk("%s: pi->log_head is NULL \n",__func__);
                return;
	}
        curr_block = pi->log_head;
        /* The inode is invalid now, no need to call PCOMMIT */
        pi->log_head = pi->log_tail = 0;

#if defined(YET_TO_BE)
        //nova_flush_buffer(&pi->log_head, CACHELINE_SIZE, 0);
#endif

	 //printk("%s: calling crfss_free_contiguous_log_blocks \n", 
	//	__func__);

        freed = crfss_free_contiguous_log_blocks(sb, pi, curr_block);

}



/*
 * NOTE! When we get the inode, we're the only people
 * that have access to it, and as such there are no
 * race conditions we have to worry about. The inode
 * is not on the hash-lists, and it cannot be reached
 * through the filesystem because the directory entry
 * has been deleted earlier.
 */
static int devfss_free_inode(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct devfss_inode *pi;
	int err = 0;
	//struct crfss_inode *sih = DEVFS_I(inode);

	//printk("%s: entering devfss_free_inode \n",__func__);

	pi = devfss_get_inode(sb, inode);

	if (pi->valid) {
		printk("%s: inode %lu still valid\n",
				__func__, inode->i_ino);
		pi->valid = 0;
	}

	if (pi->nova_ino != inode->i_ino) {
		printk("%s: inode %lu ino does not match: %llu\n",
				__func__, inode->i_ino, pi->nova_ino);
		/*printk("inode size %llu, pi addr 0x%lx, pi head 0x%llx, "
				"tail 0x%llx, mode %u\n",
				inode->i_size, sih->pi_addr, pi->log_head,
				pi->log_tail, pi->i_mode);
		printk("sih: ino %lu, inode size %lu, mode %u, "
				"inode mode %u\n", sih->ino, sih->i_size,
				sih->i_mode, inode->i_mode);
		nova_print_inode_log(sb, inode);*/
	}

#if defined(YET_TO_BE_DEFINED)
	printk("%s: Calling crfss_free_inode_log \n",__func__);
	crfss_free_inode_log(sb, pi);
#endif
	pi->i_blocks = 0;

#if defined(YET_TO_BE_DEFINED)
	sih->log_pages = 0;
	sih->i_mode = 0;
	sih->pi_addr = 0;
	sih->i_size = 0;
#endif
	err = crfss_free_inuse_inode(sb, pi->nova_ino);

	return err;
}



int crfss_get_inode_address(struct super_block *sb, u64 ino,
	u64 *pi_addr, int extendable)
{
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	struct devfss_inode *pi;
	struct inode_table *inode_table;
	unsigned int data_bits;
	unsigned int num_inodes_bits;
	u64 curr;
	unsigned int superpage_count;
	u64 internal_ino;
	int cpuid;
	unsigned int index;
	unsigned int i = 0;
	unsigned long blocknr;
	unsigned long curr_addr;
	int allocated;

	pi = crfss_get_inode_by_ino(sb, DEVFS_INODETABLE_INO);
	data_bits = crfss_blk_type_to_shift[pi->i_blk_type];
	num_inodes_bits = data_bits - DEVFS_INODE_BITS;

	cpuid = ino % sbi->cpus;
	internal_ino = ino / sbi->cpus;
	inode_table = crfss_get_inode_table(sb, cpuid);
	superpage_count = internal_ino >> num_inodes_bits;
	index = internal_ino & ((1 << num_inodes_bits) - 1);

	//printk("superpage_count %u, internal_ino %llu, num_inodes_bits %u "
	//	"index %u \n",superpage_count, internal_ino, num_inodes_bits, 
	//	index);
	curr = inode_table->log_head;
	if (curr == 0)
		return -EINVAL;

	for (i = 0; i < superpage_count; i++) {

		if (curr == 0)
			return -EINVAL;

		curr_addr = (unsigned long)crfss_get_block(sb, curr);

		/* Next page pointer in the last 8 bytes of the superpage */
		curr_addr += 4096 - 8;
		curr = *(u64 *)(curr_addr);

		//struct crfss_sb_info *sbi = DEVFS_SB(sb);
	        /*printk("superpage_count %u, internal_ino %llu, num_inodes_bits %u "
           	     "index %u curr_addr %lu, curr %llu sbi->virtaddr %lu\n",
			superpage_count, internal_ino, num_inodes_bits, index, 
			curr_addr, curr, sbi->virt_addr);*/

		if (curr == 0) {
			if (extendable == 0)
				return -EINVAL;

			allocated = crfss_new_log_blocks(sb, pi, &blocknr,
							1, 1);
			if (allocated != 1) {
				printk("crfss_new_log_blocks allocated %d\n", allocated);
				return allocated;
			}

			curr = crfss_get_block_off(sb, blocknr,
						pi->i_blk_type);
			*(u64 *)(curr_addr) = curr;
			crfss_flush_buffer((void *)curr_addr,
						DEVFS_INODE_SIZE, 1);
		}
	}

	*pi_addr = curr + index * DEVFS_INODE_SIZE;

	return 0;
}


/* Returns 0 on failure */
u64 devfss_new_crfss_inode(struct super_block *sb, u64 *pi_addr)
{
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	struct inode_map *inode_map;
	unsigned long free_ino = 0;
	int map_id;
	u64 ino = 0;
	int ret;

	map_id = sbi->map_id;
	sbi->map_id = (sbi->map_id + 1) % sbi->cpus;

#if defined(_DEVFS_DEBUG)
	printk("%s:%d sbi->map_id %lu\n", __FUNCTION__,
		__LINE__, sbi->map_id);	
#endif

	inode_map = &sbi->inode_maps[map_id];

	mutex_lock(&inode_map->inode_table_mutex);

	ret = crfss_alloc_unused_inode(sb, map_id, &free_ino);
	if (ret) {
		printk("%s: alloc inode number failed %d\n", __func__, ret);
		mutex_unlock(&inode_map->inode_table_mutex);
		return 0;
	}

	ret = crfss_get_inode_address(sb, free_ino, pi_addr, 1);
	if (ret) {
		printk("%s: get inode address failed %d\n", __func__, ret);
		mutex_unlock(&inode_map->inode_table_mutex);
		return 0;
	}

#if defined(_DEVFS_DEBUG)
	printk("%s:%d physical address %llu %lu\n", __FUNCTION__,	
		__LINE__, *pi_addr, free_ino);	
#endif

	mutex_unlock(&inode_map->inode_table_mutex);

	ino = free_ino;

	return ino;
}



static void crfss_set_next_page_flag(struct super_block *sb, u64 curr_p)
{
        void *p;

        if (ENTRY_LOC(curr_p) >= LAST_ENTRY)
                return;

        p = crfss_get_block(sb, curr_p);
        crfss_set_entry_type(p, NEXT_PAGE);
        //nova_flush_buffer(p, CACHELINE_SIZE, 1);
}


static int crfss_coalesce_log_pages(struct super_block *sb,
	unsigned long prev_blocknr, unsigned long first_blocknr,
	unsigned long num_pages)
{
	unsigned long next_blocknr;
	u64 curr_block, next_page;
	struct crfss_inode_log_page *curr_page;
	int i;

	if (prev_blocknr) {
		/* Link prev block and newly allocated head block */
		curr_block = crfss_get_block_off(sb, prev_blocknr,
						DEVFS_BLOCK_TYPE_4K);
		curr_page = (struct crfss_inode_log_page *)
				crfss_get_block(sb, curr_block);
		next_page = crfss_get_block_off(sb, first_blocknr,
				DEVFS_BLOCK_TYPE_4K);
		crfss_set_next_page_address(sb, curr_page, next_page, 0);
	}

	next_blocknr = first_blocknr + 1;
	curr_block = crfss_get_block_off(sb, first_blocknr,
						DEVFS_BLOCK_TYPE_4K);
	curr_page = (struct crfss_inode_log_page *)
				crfss_get_block(sb, curr_block);
	for (i = 0; i < num_pages - 1; i++) {
		next_page = crfss_get_block_off(sb, next_blocknr,
				DEVFS_BLOCK_TYPE_4K);
		crfss_set_next_page_address(sb, curr_page, next_page, 0);
		curr_page++;
		next_blocknr++;
	}

	/* Last page */
	crfss_set_next_page_address(sb, curr_page, 0, 1);
	return 0;
}


/* Log block resides in NVMM */
int crfss_allocate_inode_log_pages(struct super_block *sb,
	struct devfss_inode *pi, unsigned long num_pages,
	u64 *new_block)
{
	unsigned long new_inode_blocknr;
	unsigned long first_blocknr;
	unsigned long prev_blocknr;
	int allocated;
	int ret_pages = 0;

	allocated = crfss_new_log_blocks(sb, pi, &new_inode_blocknr,
					num_pages, 0);

	if (allocated <= 0) {
		printk( "ERROR: no inode log page available: %lu %d\n",
			num_pages, allocated);
		return allocated;
	}
	ret_pages += allocated;
	num_pages -= allocated;
	printk("Pi %llu: Alloc %d log blocks @ 0x%lx\n",
			pi->nova_ino, allocated, new_inode_blocknr);

	/* Coalesce the pages */
	crfss_coalesce_log_pages(sb, 0, new_inode_blocknr, allocated);
	first_blocknr = new_inode_blocknr;
	prev_blocknr = new_inode_blocknr + allocated - 1;

	/* Allocate remaining pages */
	while (num_pages) {
		allocated = crfss_new_log_blocks(sb, pi,
					&new_inode_blocknr, num_pages, 0);

		printk("Alloc %d log blocks @ 0x%lx\n",
					allocated, new_inode_blocknr);
		if (allocated <= 0) {
			printk("%s: no inode log page available: "
				"%lu %d\n", __func__, num_pages, allocated);
			/* Return whatever we have */
			break;
		}
		ret_pages += allocated;
		num_pages -= allocated;
		crfss_coalesce_log_pages(sb, prev_blocknr, new_inode_blocknr,
						allocated);
		prev_blocknr = new_inode_blocknr + allocated - 1;
	}

	*new_block = crfss_get_block_off(sb, first_blocknr,
						DEVFS_BLOCK_TYPE_4K);

	return ret_pages;
}


static u64 crfss_extend_inode_log(struct super_block *sb, struct devfss_inode *pi,
	struct crfss_inode *sih, u64 curr_p)
{
	u64 new_block;
	int allocated;
	unsigned long num_pages;

	if (curr_p == 0) {
		allocated = crfss_allocate_inode_log_pages(sb, pi,
					1, &new_block);
		if (allocated != 1) {
			printk( "%s ERROR: no inode log page "
					"available\n", __func__);
			return 0;
		}
		pi->log_tail = new_block;
		//nova_flush_buffer(&pi->log_tail, CACHELINE_SIZE, 0);
		pi->log_head = new_block;
		sih->log_pages = 1;
		pi->i_blocks++;
		//nova_flush_buffer(&pi->log_head, CACHELINE_SIZE, 1);
	} else {
		num_pages = sih->log_pages >= EXTEND_THRESHOLD ?
				EXTEND_THRESHOLD : sih->log_pages;
//		printk("Before append log pages:\n");
//		nova_print_inode_log_page(sb, inode);
		allocated = crfss_allocate_inode_log_pages(sb, pi,
					num_pages, &new_block);
		printk("Link block %llu to block %llu\n",
					curr_p >> PAGE_SHIFT,
					new_block >> PAGE_SHIFT);
		if (allocated <= 0) {
			printk( "%s ERROR: no inode log page "
					"available\n", __func__);
			printk("curr_p 0x%llx, %lu pages\n", curr_p,
					sih->log_pages);
			return 0;
		}

		//crfss_inodelog_fast_gc(sb, pi, sih, curr_p,
		//				new_block, allocated);

//		printk("After append log pages:\n");
//		nova_print_inode_log_page(sb, inode);
		/* Atomic switch to new log */
//		nova_switch_to_new_log(sb, pi, new_block, num_pages);
	}
	return new_block;
}


/* For thorough GC, simply append one more page */
static u64 crfss_append_one_log_page(struct super_block *sb,
	struct devfss_inode *pi, u64 curr_p)
{
	struct crfss_inode_log_page *curr_page;
	u64 new_block;
	u64 curr_block;
	int allocated;

	allocated = crfss_allocate_inode_log_pages(sb, pi, 1, &new_block);
	if (allocated != 1) {
		printk( "%s: ERROR: no inode log page available\n",
				__func__);
		return 0;
	}

	if (curr_p == 0) {
		curr_p = new_block;
	} else {
		/* Link prev block and newly allocated head block */
		curr_block = BLOCK_OFF(curr_p);
		curr_page = (struct crfss_inode_log_page *)
				crfss_get_block(sb, curr_block);
		crfss_set_next_page_address(sb, curr_page, new_block, 1);
	}

	return curr_p;
}


u64 crfss_get_append_head(struct super_block *sb, struct devfss_inode *pi,
	struct crfss_inode *sih, u64 tail, size_t size, int *extended)
{
	u64 curr_p;

	if(!pi) {
                printk( "%s: ERROR: pi NULL\n", __func__);
		return 0;	
	}

	if (tail) {
		crfss_dbgv("%s:%d\n",__FUNCTION__,__LINE__);
		curr_p = tail;
	}
	else {
		crfss_dbgv("%s:%d\n",__FUNCTION__,__LINE__);
		curr_p = pi->log_tail;
	}

	if (curr_p == 0 || (is_last_entry(curr_p, size) &&
				next_log_page(sb, curr_p) == 0)) {

		crfss_dbgv("%s:%d\n",__FUNCTION__,__LINE__);

		if (is_last_entry(curr_p, size))
			crfss_set_next_page_flag(sb, curr_p);

		if (sih) {

			crfss_dbgv("%s:%d\n",__FUNCTION__,__LINE__);
			curr_p = crfss_extend_inode_log(sb, pi, sih, curr_p);
		} else {

			crfss_dbgv("%s:%d\n",__FUNCTION__,__LINE__);
			curr_p = crfss_append_one_log_page(sb, pi, curr_p);
			/* For thorough GC */
			*extended = 1;
		}

		if (curr_p == 0)
			return 0;
	}

	if (is_last_entry(curr_p, size)) {
		crfss_set_next_page_flag(sb, curr_p);
		curr_p = next_log_page(sb, curr_p);
	}

	return  curr_p;
}
#endif

