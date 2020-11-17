/* 
 * The following APIs are changed from kernel 3.9 to kernel 4.8
 *
 * mem_cgroup_cache_charge() -> mem_cgroup_try_charge()
 * mem_cgroup_uncharge_cache_page() -> mem_cgroup_cancel_charge()
 * __set_page_locked() -> __SetPageLocked()
 * __clear_page_locked() -> __ClearPageLocked()
 * max_sane_readahead() is eliminated in kernel 4.8
 * radix_tree_next_hole() -> page_cache_next_hole()
 * page_cache_alloc_readahead() is deprecated in kernel 4.8, kernel 4.8 is using __page_cache_alloc() instead
 *
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

#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>
#include <xen/xen.h>
#include <xen/interface/xen.h>
#include <xen/interface/memory.h>
#include <xen/balloon.h>
#include <xen/heteromem.h>
#include <xen/features.h>
#include <xen/page.h>



/*
 * Set the initial window size, round to next power of 2 and square
 * for small size, x 4 for medium, and x 2 for large
 * for 128k (32 page) max ra
 * 1-8 page = 32k initial, > 8 page = 128k initial
 */
static unsigned long get_init_ra_size(unsigned long size, unsigned long max)
{
        unsigned long newsize = roundup_pow_of_two(size);

        if (newsize <= max / 32)
                newsize = newsize * 4;
        else if (newsize <= max / 4)
                newsize = newsize * 2;
        else
                newsize = max;

        return newsize;
}

/*
 *  Get the previous window size, ramp it up, and
 *  return it as the new window size.
 */
static unsigned long get_next_ra_size(struct file_ra_state *ra,
                                                unsigned long max)
{
        unsigned long cur = ra->size;
        unsigned long newsize;

        if (cur < max / 16)
                newsize = 4 * cur;
        else
                newsize = 2 * cur;

        return min(newsize, max);
}

/*
 * __do_page_cache_readahead() actually reads a chunk of disk.  It allocates all
 * the pages first, then submits them all for I/O. This avoids the very bad
 * behaviour which would occur if page allocations are causing VM writeback.
 * We really don't want to intermingle reads and writes like that.
 *
 * Returns the number of pages requested, or the maximum amount of I/O allowed.
 */
static int
__do_page_cache_readahead(struct address_space *mapping, struct file *filp,
			pgoff_t offset, unsigned long nr_to_read,
			unsigned long lookahead_size, struct crfss_inode *ei)
{
	struct inode *inode = mapping->host;
	struct page *page;
	unsigned long end_index;	/* The last page we want to read */
	LIST_HEAD(page_pool);
	int page_idx;
	int ret = 0;
	loff_t isize = i_size_read(inode);
	gfp_t gfp_mask = readahead_gfp_mask(mapping);

	if (isize == 0)
		goto out;

	end_index = ((isize - 1) >> PAGE_CACHE_SHIFT);

	/*
	 * Preallocate as many pages as we will need.
	 */
	for (page_idx = 0; page_idx < nr_to_read; page_idx++) {
		pgoff_t page_offset = offset + page_idx;

		if (page_offset > end_index)
			break;

		rcu_read_lock();

#if defined(_DEVFS_USE_INODE_PGTREE)
		page = radix_tree_lookup(&inode->page_tree, page_offset);
#else
		page = radix_tree_lookup(&mapping->page_tree, page_offset);
#endif

		rcu_read_unlock();
		if (page)
			continue;
#ifdef HETEROMEM
#endif
		// page = page_cache_alloc_readahead(mapping);
		page = __page_cache_alloc(gfp_mask);
		if (!page)
			break;
		page->index = page_offset;
		list_add(&page->lru, &page_pool);
		if (page_idx == nr_to_read - lookahead_size)
			SetPageReadahead(page);
		ret++;
	}

	/*
	 * Now start the IO.  We ignore I/O errors - if the page is not
	 * uptodate then the caller will launch readpage again, and
	 * will then handle the error.
	 */
	if (ret)
		read_pages(mapping, filp, &page_pool, ret, gfp_mask);
	BUG_ON(!list_empty(&page_pool));
out:
	return ret;
}

/*
 * A minimal readahead algorithm for trivial sequential/random reads.
 */
unsigned long
crfss_ondemand_readahead(struct address_space *mapping,
		   struct file_ra_state *ra, struct file *filp,
		   bool hit_readahead_marker, pgoff_t offset,
		   unsigned long req_size, struct crfss_inode *ei)
{
	//unsigned long max = max_sane_readahead(ra->ra_pages);
	unsigned long max = ra->ra_pages;
#if defined(_DEVFS_USE_INODE_PGTREE)
	struct inode *inode = mapping->host;
#endif

	/*
	 * start of file
	 */
	if (!offset)
		goto initial_readahead;

	/*
	 * It's the expected callback offset, assume sequential access.
	 * Ramp up sizes, and push forward the readahead window.
	 */
	if ((offset == (ra->start + ra->size - ra->async_size) ||
	     offset == (ra->start + ra->size))) {
		ra->start += ra->size;
		ra->size = get_next_ra_size(ra, max);
		ra->async_size = ra->size;
		goto readit;
	}

	/*
	 * Hit a marked page without valid readahead state.
	 * E.g. interleaved reads.
	 * Query the pagecache for async_size, which normally equals to
	 * readahead size. Ramp it up and use it as the new readahead size.
	 */
	if (hit_readahead_marker) {
		pgoff_t start;

		rcu_read_lock();

#if defined(_DEVFS_USE_INODE_PGTREE)
		//FIXME
                //start = radix_tree_next_hole(&inode->page_tree, offset+1,max);
		start = page_cache_next_hole(inode->i_mapping, offset+1, max);
#else
		//start = radix_tree_next_hole(&mapping->page_tree, offset+1,max);
		start = page_cache_next_hole(mapping, offset+1, max);
#endif

		printk(KERN_ALERT "page_tree %s:%d \n",__FUNCTION__,__LINE__);
		rcu_read_unlock();

		if (!start || start - offset > max)
			return 0;

		ra->start = start;
		ra->size = start - offset;	/* old async_size */
		ra->size += req_size;
		ra->size = get_next_ra_size(ra, max);
		ra->async_size = ra->size;
		goto readit;
	}

	/*
	 * oversize read
	 */
	if (req_size > max)
		goto initial_readahead;

	/*
	 * sequential cache miss
	 */
	if (offset - (ra->prev_pos >> PAGE_CACHE_SHIFT) <= 1UL)
		goto initial_readahead;

	/*
	 * Query the page cache and look for the traces(cached history pages)
	 * that a sequential stream would leave behind.
	 */
	if (try_context_readahead(mapping, ra, offset, req_size, max))
		goto readit;

	/*
	 * standalone, small random read
	 * Read as is, and do not pollute the readahead state.
	 */
	return __do_page_cache_readahead(mapping, filp, offset, req_size, 0, ei);

#if 1
initial_readahead:
	ra->start = offset;
	ra->size = get_init_ra_size(req_size, max);
	ra->async_size = ra->size > req_size ? ra->size - req_size : ra->size;

readit:
	/*
	 * Will this read hit the readahead marker made by itself?
	 * If so, trigger the readahead marker hit now, and merge
	 * the resulted next readahead window into the current one.
	 */
	if (offset == ra->start && ra->size == ra->async_size) {
		ra->async_size = get_next_ra_size(ra, max);
		ra->size += ra->async_size;
	}

	return ra_submit(ra, mapping, filp);
#endif
}


/**
 * page_cache_async_readahead - file readahead for marked pages
 * @mapping: address_space which holds the pagecache and I/O vectors
 * @ra: file_ra_state which holds the readahead state
 * @filp: passed on to ->readpage() and ->readpages()
 * @page: the page at @offset which has the PG_readahead flag set
 * @offset: start offset into @mapping, in pagecache page-sized units
 * @req_size: hint: total size of the read which the caller is performing in
 *            pagecache pages
 *
 * page_cache_async_readahead() should be called when a page is used which
 * has the PG_readahead flag; this is a marker to suggest that the application
 * has used up enough of the readahead window that we should start pulling in
 * more pages.
 */
void
crfss_read_page(struct address_space *mapping,
	struct file_ra_state *ra, struct file *filp,
	struct page *page, pgoff_t offset,
	unsigned long req_size)
{

	struct inode *inode = NULL;
	struct crfss_inode *ei = NULL;

	inode = filp->f_inode;
	ei = DEVFS_I(inode);

	/* no read-ahead */
	if (!ra->ra_pages)
		return;

	/*
	 * Same bit is used for PG_readahead and PG_reclaim.
	 */
	if (PageWriteback(page))
		return;

	ClearPageReadahead(page);

	/*
	 * Defer asynchronous read-ahead on IO congestion.
	 */
	if (bdi_read_congested(mapping->backing_dev_info))
		return;

	/* do read-ahead */
	crfss_ondemand_readahead(mapping, ra, filp, true, offset, req_size, ei);
}
EXPORT_SYMBOL_GPL(crfss_read_page);


/**
 * crfss_page_cache_async_readahead - file readahead for marked pages
 * @mapping: address_space which holds the pagecache and I/O vectors
 * @ra: file_ra_state which holds the readahead state
 * @filp: passed on to ->readpage() and ->readpages()
 * @page: the page at @offset which has the PG_readahead flag set
 * @offset: start offset into @mapping, in pagecache page-sized units
 * @req_size: hint: total size of the read which the caller is performing in
 *            pagecache pages
 *
 * crfss_page_cache_async_readahead() should be called when a page is used which
 * has the PG_readahead flag; this is a marker to suggest that the application
 * has used up enough of the readahead window that we should start pulling in
 * more pages.
 */
void
crfss_page_cache_async_readahead(struct address_space *mapping,
                           struct file_ra_state *ra, struct file *filp,
                           struct page *page, pgoff_t offset,
                           unsigned long req_size)
{

        struct inode *inode = NULL;
        struct crfss_inode *ei = NULL;

        inode = filp->f_inode;
        ei = DEVFS_I(inode);

        /* no read-ahead */
        if (!ra->ra_pages)
                return;
        /*
         * Same bit is used for PG_readahead and PG_reclaim.
         */
        if (PageWriteback(page))
                return;

        ClearPageReadahead(page);
        /*
         * Defer asynchronous read-ahead on IO congestion.
         */
        if (bdi_read_congested(mapping->backing_dev_info))
                return;

        /* do read-ahead */
        crfss_ondemand_readahead(mapping, ra, filp, true, offset, req_size, ei);
}
EXPORT_SYMBOL_GPL(crfss_page_cache_async_readahead);


/**
 * add_to_page_cache_locked - add a locked page to the pagecache
 * @page:       page to add
 * @mapping:    the page's address_space
 * @offset:     page index
 * @gfp_mask:   page allocation mode
 *
 * This function is used to add a page to the pagecache. It must be locked.
 * This function does not add the page to the LRU.  The caller must do that.
 */
int crfss_add_page_cache_locked(struct page *page, struct address_space *mapping,
                pgoff_t offset, gfp_t gfp_mask, struct crfss_inode *ei)
{
        int error;
	struct mem_cgroup *memcg;
#if defined(_DEVFS_USE_INODE_PGTREE)
	struct inode *inode = mapping->host;	
#endif

        VM_BUG_ON(!PageLocked(page));
        VM_BUG_ON(PageSwapBacked(page));

        //error = mem_cgroup_cache_charge(page, current->mm, gfp_mask & GFP_RECLAIM_MASK);
	error = mem_cgroup_try_charge(page, current->mm, gfp_mask & GFP_RECLAIM_MASK, &memcg, false);


        if (error)
                goto out;

#ifdef _DEVFS_DEBUG_RDWR
	printk(KERN_ALERT "Before %s:%d \n",__FUNCTION__,__LINE__);
#endif

        error = radix_tree_preload(gfp_mask & ~__GFP_HIGHMEM);
        if (error == 0) {

                page_cache_get(page);
                page->mapping = mapping;
                page->index = offset;

#ifdef _DEVFS_DEBUG_RDWR
		printk(KERN_ALERT "Before %s:%d \n",__FUNCTION__,__LINE__);
#endif
                spin_lock_irq(&mapping->tree_lock);

#ifdef _DEVFS_DEBUG_RDWR
		printk(KERN_ALERT "After %s:%d \n",__FUNCTION__,__LINE__);
#endif


#if defined(_DEVFS_USE_INODE_PGTREE)
		error = radix_tree_insert(&inode->page_tree, offset, page);
#else
                error = radix_tree_insert(&mapping->page_tree, offset, page);
#endif

                if (likely(!error)) {
                        mapping->nrpages++;
                        __inc_zone_page_state(page, NR_FILE_PAGES);
                        spin_unlock_irq(&mapping->tree_lock);
		
			//printk(KERN_ALERT "Success %s:%d off %lu\n",
                          //      __FUNCTION__,__LINE__, offset);
               } else {
                        page->mapping = NULL;
                        /* Leave page->index set: truncation relies upon it */
                        spin_unlock_irq(&mapping->tree_lock);
                        //mem_cgroup_uncharge_cache_page(page);
			mem_cgroup_cancel_charge(page, memcg, false);
                        page_cache_release(page);
                }
                radix_tree_preload_end();
		//radix_tree_preload_end();
        } else {
		printk(KERN_ALERT "radix_tree_preload failed %s:%d \n",
				__FUNCTION__,__LINE__);		
	        //mem_cgroup_uncharge_cache_page(page);
		mem_cgroup_cancel_charge(page, memcg, false);
	}
out:
#ifdef _DEVFS_DEBUG_RDWR
	printk(KERN_ALERT "Finishing %s:%d \n",__FUNCTION__,__LINE__);
#endif

        return error;
}
EXPORT_SYMBOL(crfss_add_page_cache_locked);

/*
 * Like add_to_page_cache_locked, but used to add newly allocated pages:
 * the page is new, so we can just run __SetPageLocked() against it.
 */
static inline int crfss_add_to_page_cache(struct page *page,
                struct address_space *mapping, pgoff_t offset, gfp_t gfp_mask, 
		struct crfss_inode *ei)
{
        int error;

        __SetPageLocked(page);
        error = crfss_add_page_cache_locked(page, mapping, offset, gfp_mask, ei);
        if (unlikely(error))
                __ClearPageLocked(page);
        return error;
}

int crfss_add_page_lru(struct page *page, struct address_space *mapping,
                          pgoff_t offset, gfp_t gfp_mask, struct crfss_inode *ei)
{
        int ret;

        ret = crfss_add_to_page_cache(page, mapping, offset, gfp_mask, ei);
        if (ret == 0)
                lru_cache_add_file(page);

        return ret;
}
EXPORT_SYMBOL_GPL(crfss_add_page_lru);


struct page *crfss_get_cache_page(struct address_space *mapping,
	pgoff_t index, unsigned flags, struct crfss_inode *ei, 
	struct inode *inode)
{
        int status;
        gfp_t gfp_mask;
        struct page *page;
        gfp_t gfp_notmask = 0;

#if defined(_DEVFS_NOVA_BASED)
        //unsigned long blocknr = 0;
        //unsigned long start_blk = 0;
        struct super_block *sb;
#endif
        gfp_mask = mapping_gfp_mask(mapping);

#if !defined(_DEVFS_CHECK)
        if (mapping_cap_account_dirty(mapping))
                gfp_mask |= __GFP_WRITE;
        if (flags & AOP_FLAG_NOFS)
                gfp_notmask = __GFP_FS;
#endif
	//printk(KERN_ALERT "Start %s:%d %lu\n",
	//		__FUNCTION__,__LINE__,index);

#if defined(_DEVFS_NOVA_BASED)
        sb = inode->i_sb;
        //crfss_new_data_blocks(sb, NULL, &blocknr, 1, start_blk, 0, 0);
#endif

#if !defined(_DEVFS_CHECK)
        page = find_lock_page(mapping, index);
        if (page)
                goto found;
#endif

#if defined(_DEVFS_MEMGMT)
	//page = hetero_alloc_IO(gfp_mask & ~gfp_notmask, 0, 0);
	page = crfss_alloc_page(gfp_mask & ~gfp_notmask, 0, 0);
	if(!page) {
		printk("hetero_alloc failed \n");	
		page = __page_cache_alloc(gfp_mask & ~gfp_notmask);
	}
#else
        page = __page_cache_alloc(gfp_mask & ~gfp_notmask);
#endif
        if (!page)
                return NULL;

        status = crfss_add_page_lru(page, mapping, index,
                               GFP_KERNEL & ~gfp_notmask, ei);
        if (unlikely(status)) {

                page_cache_release(page);
                if (status == -EEXIST) {
			printk(KERN_ALERT "%s:%d failed, already exists \n",
				__FUNCTION__,__LINE__);	
                        //goto repeat;
		}
                return NULL;
        }
	//printk(KERN_ALERT "Repeat DONE %s:%d %lu\n",
	//		__FUNCTION__,__LINE__,index);
#if !defined(_DEVFS_CHECK)
        wait_for_stable_page(page);
#endif
        return page;
}
EXPORT_SYMBOL(crfss_get_cache_page);


#if 0

/**
 * find_get_page - find and get a page reference
 * @mapping: the address_space to search
 * @offset: the page index
 *
 * Is there a pagecache struct page at the given (mapping, offset) tuple?
` * If yes, increment its refcount and return it; if no, return NULL.
 */
struct page *crfss_find_get_page(struct address_space *mapping, pgoff_t offset, 
					struct crfss_inode *ei)
{
        void **pagep;
        struct page *page;
	struct inode *inode = mapping->host;

        rcu_read_lock();
repeat:
        page = NULL;


#if defined(_DEVFS_USE_INODE_PGTREE)
	pagep = radix_tree_lookup_slot(&inode->page_tree, offset);
#else
        pagep = radix_tree_lookup_slot(&mapping->page_tree, offset);
#endif
	if(!pagep){
		printk(KERN_ALERT "Failed %s:%d off %lu\n",
				__FUNCTION__,__LINE__, offset);	
		return NULL;
	}

        if (pagep) {

                page = radix_tree_deref_slot(pagep);

                if (unlikely(!page))
                        goto out;

                if (radix_tree_exception(page)) {
                        if (radix_tree_deref_retry(page))
                                goto repeat;
                        /*
                         * Otherwise, shmem/tmpfs must be storing a swap entry
                         * here as an exceptional entry: so return it without
                         * attempting to raise page count.
                         */
                        goto out;
                }

#if 1// !defined(_DEVFS_CHECK)
                if (!page_cache_get_speculative(page))
                        goto repeat;
#endif

                /*
                 * Has the page moved?
                * This is part of the lockless pagecache protocol. See
                 * include/linux/pagemap.h for details.
                 */
                if (unlikely(page != *pagep)) {
                        page_cache_release(page);
                        goto repeat;
                }
        }
out:
        rcu_read_unlock();

        return page;
}
EXPORT_SYMBOL(crfss_find_get_page);
#endif


/**
 * find_get_page - find and get a page reference
 * @mapping: the address_space to search
 * @offset: the page index
 *
 * Is there a pagecache struct page at the given (mapping, offset) tuple?
` * If yes, increment its refcount and return it; if no, return NULL.
 */
struct page *crfss_find_get_page(struct address_space *mapping, pgoff_t offset, 
					struct crfss_inode *ei)
{
        void **pagep;
        struct page *page;
	//struct inode *inode = mapping->host;

        rcu_read_lock();
repeat:
        page = NULL;

        pagep = radix_tree_lookup_slot(&mapping->page_tree, offset);
	//pagep = radix_tree_lookup_slot(&inode->page_tree, offset);
	if(!pagep){
		printk(KERN_ALERT "Failed %s:%d off %lu\n",
				__FUNCTION__,__LINE__, offset);	
		return NULL;
	}

        if (pagep) {

                page = radix_tree_deref_slot(pagep);

                if (unlikely(!page))
                        goto out;

                if (radix_tree_exception(page)) {
                        if (radix_tree_deref_retry(page))
                                goto repeat;
                        /*
                         * Otherwise, shmem/tmpfs must be storing a swap entry
                         * here as an exceptional entry: so return it without
                         * attempting to raise page count.
                         */
                        goto out;
                }

#if 1// !defined(_DEVFS_CHECK)
                if (!page_cache_get_speculative(page))
                        goto repeat;
#endif

                /*
                 * Has the page moved?
                * This is part of the lockless pagecache protocol. See
                 * include/linux/pagemap.h for details.
                 */
                if (unlikely(page != *pagep)) {
                        page_cache_release(page);
                        goto repeat;
                }
        }
out:
        rcu_read_unlock();

        return page;
}
EXPORT_SYMBOL(crfss_find_get_page);


