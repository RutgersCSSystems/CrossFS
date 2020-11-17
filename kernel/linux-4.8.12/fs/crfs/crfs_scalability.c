/*
 * devfs_scalability.c
 *
 * Description: Scalability related functions
 *
 */
#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/file.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/nvme.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>

DEFINE_MUTEX(ei_mutex);

/* 
 * Clean per-file-pointer queue buffer, flush to storage
 *
 * Called when a host thread is closing this file pointer
 */
void crfss_scalability_flush_buffer(struct crfss_fstruct *rd) {
	/* 
	 * need to wait concurrent writers insert to
	 * this rd on a conflict update of same block
	 */
	while (test_and_set_bit(1, &rd->closed) == 1);

	//rd->closed = 1; 
	test_and_set_bit(0, &rd->closed);

	/* wait for device thread to flush buffer to storage */
	while(test_and_clear_bit(0, &rd->io_done) == 0);

}
EXPORT_SYMBOL(crfss_scalability_flush_buffer);


/* 
 * Setup per-file-pointer scalability data structures 
 * when a file is opened
 */
int crfss_scalability_open_setup(struct crfss_inode *ei, struct crfss_fstruct *rd) {
	int retval = 0, i = 0;
	struct inode *inode = NULL;

	inode = rd->fp->f_inode;

	if (inode->sq_tree_init != _DEVFS_INITIALIZE) {

		rwlock_init(&ei->i_meta_lock);
		INIT_RADIX_TREE(&ei->sq_tree, GFP_ATOMIC);
		/* init submission queue radix tree lock */
		__SPIN_LOCK_UNLOCKED(ei->sq_tree_lock);
		ei->sq_tree_init = _DEVFS_INITIALIZE;

		if(rd && rd->fp) {
			inode->sq_tree_init = _DEVFS_INITIALIZE;
			INIT_RADIX_TREE(&inode->sq_tree, GFP_ATOMIC);
			__SPIN_LOCK_UNLOCKED(inode->sq_tree_lock);
		}

		for (i = 0; i < MAX_FP_QSIZE; ++i)
			ei->per_rd_queue[i] = NULL;

	}

	/* init per file pointer queue */
	read_lock(&ei->i_meta_lock);
	if (ei->rd_nr >= MAX_FP_QSIZE) {
		printk(KERN_ALERT "DEBUG: per file pointer queue limit excced,"
			"ei = %llx, rd_nr = %d | %s:%d",
			(long long unsigned int)ei, ei->rd_nr,
			 __FUNCTION__, __LINE__);
		retval = -EFAULT;
		read_unlock(&ei->i_meta_lock);
		goto err_open_setup;
	}
	read_unlock(&ei->i_meta_lock);

	write_lock(&ei->i_meta_lock);
	ei->per_rd_queue[ei->rd_nr] = rd;
	rd->index = ei->rd_nr;
	++ei->rd_nr;
	write_unlock(&ei->i_meta_lock);

	/* init rd close flag */
	rd->closed = 0;

	/* init io return bytes */
	rd->iobytes = 0;

	/* init io notify */
	rd->io_done = 0;

	/* init rd state */
	rd->state = DEVFS_RD_IDLE;

	/* initialize list structure of this rd */
	INIT_LIST_HEAD(&rd->list);

	/* initialize cmd queue pointer */
	rd->req = NULL;

	/* initialize fsyncing flag */
	rd->fsyncing = 0;

	/* 
	 * Add this rd to the global rd linked list
	 * The reason we need mutex lock is that
	 * there could be multiple host threads tries to insert
	 * its rd struct to the global rd linked list
	 * at the same time
	 */
	if (crfss_scheduler_add_list(rd)) {
		printk(KERN_ALERT "insert rd list failed | %s:%d",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;
		goto err_open_setup;
	}

err_open_setup:
	return retval;

}
EXPORT_SYMBOL(crfss_scalability_open_setup);

// Setup per-file-pointer scalability data structures when a file is closed
void crfss_scalability_close_setup(struct crfss_inode *ei, struct crfss_fstruct *rd) {
	/* decrement inode rd count */
	write_lock(&ei->i_meta_lock);
	if (ei->rd_nr > 0) {
		ei->per_rd_queue[rd->index] = NULL;	
		--ei->rd_nr;
	}
	write_unlock(&ei->i_meta_lock);

	if (crfss_scheduler_del_list(rd)) {
		printk(KERN_ALERT "delete rd list failed | %s:%d",
			__FUNCTION__, __LINE__);
	}

}
EXPORT_SYMBOL(crfss_scalability_close_setup);


/* 
 * Search block in per-inode submission tree
 * for read operation, read search will
 * copy data directly to user buffer if
 * all the data block are present
 */
int crfss_read_submission_tree_search (struct inode *inode, nvme_cmdrw_t *cmdrw,
                                        struct crfss_fstruct **target_rd) {

	struct req_tree_entry *tree_node = NULL;
	int retval = -1;

#ifdef _DEVFS_DISABLE_SQ
    return DEVFS_SUBMISSION_TREE_NOTFOUND;
#endif

#ifndef _DEVFS_INTERVAL_TREE
	size_t itr = 0;
	int hit_nr = 0, blk_nr = 0;
	loff_t index = 0;

	for (itr = 0; itr < cmdrw->nlb; itr += PAGE_SIZE) {
		index = (loff_t)cmdrw->slba + itr >> PAGE_CACHE_SHIFT;

		/* search if current block is in radix tree */
		spin_lock(&inode->sq_tree_lock);
		tree_node = radix_tree_lookup(&inode->sq_tree, index);
		spin_unlock(&inode->sq_tree_lock);

		if (tree_node) {
			/* increment hit count */
			++hit_nr;

			if (copy_to_user((void __user *)cmdrw->common.prp2 + itr,
				 (void *)tree_node->blk_addr, tree_node->size)) {
				printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
				retval = -EFAULT;
				goto err_read_submission_tree_search;
			}

			if (*target_rd == NULL)
				*target_rd = tree_node->rd;
		}
		++blk_nr;
	}

	if (hit_nr == blk_nr) {
		/* All of the target block are found in radix tree */
		retval = DEVFS_SUBMISSION_TREE_FOUND;
	} else if (hit_nr == 0) {
		/* None of the target block are found in radix tree */
		retval = DEVFS_SUBMISSION_TREE_NOTFOUND;
	} else if (hit_nr > 0 && hit_nr < blk_nr) {
		/* Partial target block found in radix tree */
		retval = DEVFS_SUBMISSION_TREE_PARFOUND;
	}

#else
	// Interval Tree code:
	struct interval_tree_node *it;
	unsigned long start = cmdrw->slba;
	unsigned long end = cmdrw->slba + cmdrw->nlb - 1;

	spin_lock(&inode->sq_tree_lock);
	it = interval_tree_iter_first(&inode->sq_it_tree, start, end);

	if (it) {
		tree_node = container_of(it, struct req_tree_entry, it);
		if (!tree_node) {
			printk(KERN_ALERT "Get NULL object! | %s:%d\n", __FUNCTION__, __LINE__);
			retval = -EFAULT;
			goto err_read_submission_tree_search;
		}
	
		if (copy_to_user((void __user *)cmdrw->common.prp2,
			(void *)tree_node->blk_addr, tree_node->size)) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_read_submission_tree_search;
		}

		*target_rd = tree_node->rd;
		retval = DEVFS_SUBMISSION_TREE_FOUND;

#ifdef _DEVFS_STAT
		crfss_stat_fp_queue_hit();
#endif
	} else {
		retval = DEVFS_SUBMISSION_TREE_NOTFOUND;
	}
	spin_unlock(&inode->sq_tree_lock);
#endif

#ifdef _DEVFS_STAT
	crfss_stat_fp_queue_access();
#endif

err_read_submission_tree_search:
	return retval;

}

/*
 * Search block in per-inode submission tree
 * for write operation, write search will
 * only do search
 */
int crfss_write_submission_tree_search (struct inode *inode, nvme_cmdrw_t *cmdrw,
					struct crfss_fstruct **target_rd) {
	struct req_tree_entry *tree_node = NULL;
	int retval = -1;

#ifdef _DEVFS_DISABLE_SQ
    return DEVFS_SUBMISSION_TREE_NOTFOUND;
#endif

#ifndef _DEVFS_INTERVAL_TREE
	size_t itr = 0;
	int hit_nr = 0, blk_nr = 0;
	loff_t index = 0;

	for (itr = 0; itr < cmdrw->nlb; itr += PAGE_SIZE) {
		index = (loff_t)cmdrw->slba + itr >> PAGE_CACHE_SHIFT;

		/* search if current block is in radix tree */
		spin_lock(&inode->sq_tree_lock);
		tree_node = radix_tree_lookup(&inode->sq_tree, index);
		spin_unlock(&inode->sq_tree_lock);

		if (tree_node) {
			/* increment hit count */
			++hit_nr;

			if (*target_rd == NULL)
				*target_rd = tree_node->rd;
		}
		++blk_nr;
	}

	if (hit_nr > 0) {
		/* Have conflict write operations */
		retval = DEVFS_SUBMISSION_TREE_FOUND;
	} else if (hit_nr == 0) {
		/* No conflict write oprations */
		retval = DEVFS_SUBMISSION_TREE_NOTFOUND;
	}

#else
	// Interval Tree code:
	struct interval_tree_node *it;
	unsigned long start = cmdrw->slba;
	unsigned long end = cmdrw->slba + cmdrw->nlb - 1;

	spin_lock(&inode->sq_tree_lock);
	it = interval_tree_iter_first(&inode->sq_it_tree, start, end);

	if (it) {
		tree_node = container_of(it, struct req_tree_entry, it);
		if (!tree_node) {
			printk(KERN_ALERT "Get NULL object! | %s:%d\n", __FUNCTION__, __LINE__);
			retval = -EFAULT;
			goto err_write_submission_tree_search;
		}
	
		*target_rd = tree_node->rd;
		retval = DEVFS_SUBMISSION_TREE_FOUND;
	} else {
		retval = DEVFS_SUBMISSION_TREE_NOTFOUND;
	}
	spin_unlock(&inode->sq_tree_lock);
#endif

err_write_submission_tree_search:
	return retval;

}


/* Insert block in per-inode submission tree */
int crfss_write_submission_tree_insert (struct inode *inode, nvme_cmdrw_t *cmdrw,
					struct crfss_fstruct *rd) {
	int retval = 0;
	struct req_tree_entry *new_tree_node = NULL;

#ifdef _DEVFS_DISABLE_SQ
    return 0;
#endif

#ifndef _DEVFS_INTERVAL_TREE
	int itr = 0;
	loff_t index = 0;

	for (itr = 0; itr < cmdrw->nlb; itr += PAGE_SIZE) {
		index = (loff_t)cmdrw->slba + itr >> PAGE_CACHE_SHIFT;
		new_tree_node = kmalloc(sizeof(struct req_tree_entry), GFP_KERNEL);
		if (!new_tree_node) {
			printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_submission_tree_insert;
		}
		new_tree_node->blk_addr = cmdrw->blk_addr + itr;
		new_tree_node->size =
			(itr + PAGE_SIZE > cmdrw->nlb) ? cmdrw->nlb - itr : PAGE_SIZE;
		new_tree_node->rd = rd;

#ifdef _DEVFS_SCALABILITY_DBG
		printk(KERN_ALERT "rd = %llx | insert index = %lld, dest = %llx\n",
			(__u64)rd, index, (__u64)cmdrw->blk_addr);
#endif
		spin_lock(&inode->sq_tree_lock);
		radix_tree_insert(&inode->sq_tree, index, new_tree_node);
		spin_unlock(&inode->sq_tree_lock);
	}

#else
	//TODO Interval Tree code:
	new_tree_node = kmalloc(sizeof(struct req_tree_entry), GFP_KERNEL);
	if (!new_tree_node) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_submission_tree_insert;
	}

	new_tree_node->blk_addr = (void*)cmdrw->blk_addr;
	new_tree_node->size = cmdrw->nlb;
	new_tree_node->rd = rd;

	new_tree_node->it.start = cmdrw->slba;
	new_tree_node->it.last  = cmdrw->slba + cmdrw->nlb - 1;

	spin_lock(&inode->sq_tree_lock);
	interval_tree_insert(&new_tree_node->it, &inode->sq_it_tree);
	spin_unlock(&inode->sq_tree_lock);
#endif

err_submission_tree_insert:
	return retval;
}

/* Remove and de-allocate entry in per-inode submission tree */
int crfss_write_submission_tree_delete(struct inode *inode, nvme_cmdrw_t *cmdrw) {
	int retval = 0;
	struct req_tree_entry *tree_node = NULL;

#ifdef _DEVFS_DISABLE_SQ
    return 0;
#endif

#ifndef _DEVFS_INTERVAL_TREE
	int itr = 0;
	loff_t index = 0;

	/* remove entry in nv radix tree */
	for (itr = 0; itr < cmdrw->nlb; itr += PAGE_SIZE) {
		index = (loff_t)cmdrw->slba + itr >> PAGE_CACHE_SHIFT;

		spin_lock(&inode->sq_tree_lock);
		tree_node = radix_tree_delete(&inode->sq_tree, index);
		if (tree_node) {
			kfree(tree_node);
			tree_node = NULL;
		}
		spin_unlock(&inode->sq_tree_lock);

	}

#else
	// Interval Tree code:
	struct interval_tree_node *it;
	unsigned long start = cmdrw->slba;
	unsigned long end = cmdrw->slba + cmdrw->nlb - 1;

	spin_lock(&inode->sq_tree_lock);
	it = interval_tree_iter_first(&inode->sq_it_tree, start, end);
	if (it) {
		interval_tree_remove(it, &inode->sq_it_tree);

		tree_node = container_of(it, struct req_tree_entry, it);
		if (tree_node) {
			kfree(tree_node);
			tree_node = NULL;
		}
	}
	spin_unlock(&inode->sq_tree_lock);
#endif

	return retval;
}
