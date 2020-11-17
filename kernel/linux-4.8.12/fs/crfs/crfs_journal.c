#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/devfs.h>
#include <linux/fsnotify.h>
#include <linux/mount.h>
#include <linux/kernel.h>
#include <linux/devfs.h>
#include <linux/vmalloc.h>

#define DRIVER_VERSION  "0.2"
#define DRIVER_AUTHOR   "Alex Williamson <alex.williamson@redhat.com>"
#define DRIVER_DESC	 "Type1 IOMMU driver for VFIO"

uint16_t crfss_nxt_trans_id=0;

static inline uint32_t next_log_entry(uint32_t jsize, uint32_t le_off)
{
        le_off = le_off + LOGENTRY_SIZE;
        if (le_off >= jsize)
                le_off = 0;
        return le_off;
}

static inline uint32_t prev_log_entry(uint32_t jsize, uint32_t le_off)
{
        if (le_off == 0)
                le_off = jsize;
        le_off = le_off - LOGENTRY_SIZE;
        return le_off;
}

static inline uint16_t next_gen_id(uint16_t gen_id)
{
        gen_id++;
        /* check for wraparound */
        if (gen_id == 0)
                gen_id++;
        return gen_id;
}

static inline uint16_t prev_gen_id(uint16_t gen_id)
{
        gen_id--;
        /* check for wraparound */
        if (gen_id == 0)
                gen_id--;
        return gen_id;
}


void crfss_print_journ(struct crfss_inode *ei){

	crfss_journal_t *journal = ei->journal;
	if(!journal) {
		printk(KERN_ALERT "crfss_get_journal_base: ei->journal NULL\n");
		return;
	}
	printk(KERN_ALERT "journal->size %u, "
		"journal->gen_id %u, journal->, head, tailhead %u, ",
		journal->size, journal->gen_id, journal->head);
}
//EXPORT_SYMBOL(crfss_print_journ);

void crfss_print_trans(struct crfss_inode *ei, crfss_transaction_t *trans){

	if(!trans) {
		printk(KERN_ALERT "crfss_print_trans: trans NULL\n");
		return;
	}
	printk(KERN_ALERT "trans id %d, num_les %d, num_used %x\n",
		    trans->transaction_id, trans->num_entries,trans->num_used);
}

void crfss_print_logentry(crfss_logentry_t *le) {

	if(le && le->type) {
		printk(KERN_ALERT "%s:%d addr_offset %llu transct id %d, gen id %d "
			"type %d size %d\n", __FUNCTION__,__LINE__, le->addr_offset, 
			le->transaction_id, le->gen_id, le->type, le->size);
	}
	return;
}

/*Returns per process journal base data addr*/
uint64_t crfss_get_journal_base(struct crfss_inode *ei){

	uint64_t base = 0;

	if(!ei->journal) {
		printk(KERN_ALERT "crfss_get_journal_base: ei->journal NULL\n");
		return base;
	}
	base = (uint64_t)ei->journal + sizeof(crfss_journal_t);
	return base;
}

/*Returns the address of per file journal*/
crfss_journal_t *crfss_get_journal(struct crfss_inode *ei){
	return ei->journal;	
}




int crfss_journal_soft_init(struct crfss_inode *ei)
{
        crfss_journal_t *journal = crfss_get_journal(ei);

        ei->next_transaction_id = 0;

	/*Supress user-level journal options*/
	ei->isjourn = 0;

#if defined(BEFORE_DEADLINE)
        ei->journal_base_addr = crfs_get_block(sb,le64_to_cpu(journal->base));
#else
        ei->journal_base_addr = journal->base;
#endif
        ei->jsize = le32_to_cpu(journal->size);
        mutex_init(&ei->journal_mutex);

#if defined(BEFORE_DEADLINE)
        sbi->redo_log = !!le16_to_cpu(journal->redo_logging);
        return crfs_journal_cleaner_run(sb);
#endif
	return 0;
}



int crfss_journal_hard_init(struct crfss_inode *ei, uint64_t base,
	uint32_t size)
{
	crfss_journal_t *journal = crfss_get_journal(ei);
	if(!journal) {
		printk(KERN_ALERT "crfss_journal_hard_init journal NULL\n");
		return -1;
	}
	journal->base = cpu_to_le64(base);
	journal->size = cpu_to_le32(size);
	journal->gen_id = cpu_to_le16(1);
	journal->head = journal->tail = 0;
	/* lets do Undo logging for now */
	journal->redo_logging = 0;

	crfss_journal_soft_init(ei);

	return 0;
}

static int crfss_free_logentries(int max_log_entries)
{
        crfss_dbgv("crfs_free_logentries: Not Implemented\n");
        return -ENOMEM;
}


crfss_transaction_t *
crfss_new_transaction(struct crfss_inode *ei, int max_log_entries)
{

	crfss_transaction_t *trans = NULL;
	uint32_t head, tail, req_size, avail_size;
	uint64_t base;
	crfss_journal_t *journal = NULL;	

	if(!ei) {
		printk(KERN_ALERT "%s:%d ei NULL \n",__FUNCTION__,__LINE__);
		 BUG_ON(!ei);
		goto err_new_trans;
	}
	/*get per process journal structure*/
	journal = crfss_get_journal(ei);
	if(!journal) {
		printk(KERN_ALERT "journal init failed \n");
		BUG_ON(!journal);
		goto err_new_trans;
	}

	trans = alloc_transaction(ei);
	if (!trans) {
		printk(KERN_ALERT "%s:%d transaction alloc failed \n",
			__FUNCTION__,__LINE__);
		BUG_ON(!trans);	
		goto err_new_trans;
	}
	memset(trans, 0, sizeof(*trans));
	trans->num_used = 0;
	trans->num_entries = max_log_entries;
	trans->t_journal = journal;
	req_size = max_log_entries << LESIZE_SHIFT;

	mutex_lock(&ei->journal_mutex);

	tail = le32_to_cpu(journal->tail);
	head = le32_to_cpu(journal->head);
	trans->transaction_id = ei->next_transaction_id++;

again:
	trans->gen_id = le16_to_cpu(journal->gen_id);

	crfss_dbgv("crfss_new_transaction:: "
		"transaction_id %d req_size %u, trans->num_entries %d trans->num_used %u\n",
		trans->transaction_id, req_size, trans->num_entries,trans->num_used);

	avail_size = (tail >= head) ?
		(ei->jsize - (tail - head)) : (head - tail);

	avail_size = avail_size - LOGENTRY_SIZE;


	/*Check available size is greater than the request size*/
        if (avail_size < req_size) {
                uint32_t freed_size;
                /* run the log cleaner function to free some log entries */
                freed_size = crfss_free_logentries(max_log_entries);

#if defined(_DEVFS_FAKE_CLEAN)
		if ((avail_size + freed_size) < req_size) {
			avail_size = ei->jsize;
			tail = 0;
			journal->tail = journal->head;
		}
#endif
                if ((avail_size + freed_size) < req_size) {
			printk(KERN_ALERT "req_size %zu avail_size + freed_size %zu "
				"ei->jsize %u tail %u, head %u\n",
				req_size, avail_size + freed_size, ei->jsize, tail, head);
			BUG_ON((avail_size + freed_size) < req_size);
                        goto journal_full;
		}
        }

        base = le64_to_cpu(journal->base) + tail;
        tail = tail + req_size;
	
      /* journal wraparound because of this transaction allocation.
       * start the transaction from the beginning of the journal so
       * that we don't have any wraparound within a transaction */
      crfss_memunlock_range(ei, journal, sizeof(*journal));

	if (tail >= ei->jsize) {

              volatile u64 *ptr;
              tail = 0;
              /* write the gen_id and tail atomically. Use of volatile is
               * normally prohibited in kernel code, but it is required here
               * because we want to write atomically against power failures
               * and locking can't provide that. */
              ptr = (volatile u64 *)&journal->tail;
              /* writing 8-bytes atomically setting tail to 0 */
              set_64bit(ptr, (u64)cpu_to_le16(next_gen_id(le16_to_cpu(
                              journal->gen_id))) << 32);

              crfss_memlock_range(ei, journal, sizeof(*journal));

              printk(KERN_ALERT "journal wrapped. tail %x gid %d cur tid %d\n",
                      le32_to_cpu(journal->tail),le16_to_cpu(journal->gen_id),
                              ei->next_transaction_id - 1);
              goto again;

      	} else {

              journal->tail = cpu_to_le32(tail);
              crfss_memlock_range(ei, journal, sizeof(*journal));
	}
      	mutex_unlock(&ei->journal_mutex);

	avail_size = avail_size - req_size;

#if defined(BEFORE_DEADLINE)
        /* wake up the log cleaner if required */
        if ((ei->jsize - avail_size) > (ei->jsize >> 3))
                wakeup_log_cleaner(sbi);
#endif

	crfss_flush_buffer(&journal->tail, sizeof(u64), false);

	trans->start_addr = (crfss_logentry_t *)
				crfss_get_journal_base(ei);
	//TODO: When using physical address
	//trans->start_addr = crfs_get_block(sb, base);

	crfss_dbgv("created new transaction tid %d nle %d avl sz %x sa %llx\n",
		trans->transaction_id, max_log_entries, avail_size, base);

	//trans->parent = (crfss_transaction_t *)current->journal_info;
	//current->journal_info = trans;
	return trans;

journal_full:
        mutex_unlock(&ei->journal_mutex);
        printk(KERN_ALERT, "Journal full. base %llx sz %x head:tail %x:%x ncl %x\n",
                le64_to_cpu(journal->base), le32_to_cpu(journal->size),
                le32_to_cpu(journal->head), le32_to_cpu(journal->tail),
                max_log_entries);

	free_transaction(ei, trans);

err_new_trans:
	BUG_ON(!trans);
	BUG_ON(!journal);
        return NULL;
}

/* Create and return inode (metadata) transaction */
crfss_transaction_t *
crfss_new_ino_trans (struct crfss_inode *ei){

	crfss_transaction_t *trans = NULL;

	if(!ei || !ei->journal) {
		printk(KERN_ALERT "%s:%d Failed ei->journal NULL \n",
				__FUNCTION__,__LINE__);
		//return ERR_PTR(-ENOMEM);		
		BUG_ON(!ei->journal);
		return NULL;
	}	

	trans = crfss_new_transaction(ei, MAX_INODE_LENTRIES +
		            MAX_METABLOCK_LENTRIES);
	if (!trans) {
		printk(KERN_ALERT "%s:%d Failed NULL \n",
				__FUNCTION__,__LINE__);
		//return ERR_PTR(-ENOMEM);
		BUG_ON(!trans);
		return NULL;
	}
	return trans;
}
//EXPORT_SYMBOL(crfss_new_ino_trans);


/* If this is part of a read-modify-write of the block,
 * crfss_memunlock_block() before calling! */
static inline void *
crfss_get_dataaddr(struct crfss_inode *ei, u64 off)
{
	return off ? ((void *)ei->journal + off) : NULL;
}


static inline u64
crfss_get_addr_off(struct crfss_inode *ei, void *addr)
{
	return addr? (u64)(addr -(void *)ei->journal) : 0;
}


static inline void invalidate_gen_id(crfss_logentry_t *le)
{
        le->gen_id = 0;
        crfss_flush_buffer(le, LOGENTRY_SIZE, false);
}



/* can be called by either during log cleaning or during journal recovery */
static void crfss_flush_transaction(struct crfss_inode *ei,
		crfss_transaction_t *trans)
{
	crfss_logentry_t *le = trans->start_addr;
	int i;
	char *data;

	if(!trans) {
		printk(KERN_ALERT "crfss_flush_transaction NULL trans\n");
		return;
		}
	if(!le) {
		printk(KERN_ALERT "le pointer NULL, error \n");
		return;
	}

	for (i = 0; i < trans->num_used; i++, le++) {
	    if (le->size) {
		data = crfss_get_dataaddr(ei,le64_to_cpu(le->addr_offset));
		if(data)	
		    crfss_flush_buffer(data, le->size, false);
	    }
	}
}

static inline void crfss_commit_logentry(struct crfss_inode *ei,
		crfss_transaction_t *trans, crfss_logentry_t *le)
{
		/* Undo Log */
		/* Update the FS in place: currently already done. so
		 * only need to clflush */
		crfss_flush_transaction(ei, trans);
		DEVFS_PERSISTENT_MARK();
		DEVFS_PERSISTENT_BARRIER();
		/* Atomically write the commit type */
		le->type |= LE_COMMIT;
		barrier();
		/* Atomically make the log entry valid */
		le->gen_id = cpu_to_le16(trans->gen_id);
		crfss_flush_buffer(le, LOGENTRY_SIZE, true);
}

int crfss_add_logentry(struct crfss_inode *ei,
		crfss_transaction_t *trans, void *addr, uint16_t size, u8 type)
{
	crfss_logentry_t *le;
	int num_les = 0, i;
	uint64_t le_start = size ? crfss_get_addr_off(ei, addr) : 0;
	uint8_t le_size;

	if (trans == NULL)
		return -EINVAL;
	le = trans->start_addr + trans->num_used;

	printk(KERN_ALERT "addr = %llx | %s:%d\n", addr, __FUNCTION__, __LINE__);

	if (size == 0) {
		/* At least one log entry required for commit/abort log entry */
		if ((type & LE_COMMIT) || (type & LE_ABORT)) {
		    num_les = 1;
	}
	} else {
		num_les = (size + sizeof(le->data) - 1)/sizeof(le->data);
	}
	for (i = 0; i < num_les; i++) {
		le->addr_offset = cpu_to_le64(le_start);
		le->transaction_id = cpu_to_le32(trans->transaction_id);
		le_size = (i == (num_les - 1)) ? size : sizeof(le->data);
		le->size = le_size;
		size -= le_size;
		if (le_size)
		    memcpy(le->data, addr, le_size);
		le->type = type;

		if (i == 0 && trans->num_used == 0)
		    le->type |= LE_START;
		trans->num_used++;

		/* handle special log entry */
		if (i == (num_les - 1) && (type & LE_COMMIT)) {

			if(trans)
				crfss_dbgv("LE_COMMIT trans->id %u\n", 
				trans->transaction_id);

		    crfss_commit_logentry(ei, trans, le);

		   return 0;
		}
		/* put a compile time barrier so that compiler doesn't reorder
		 * the writes to the log entry */
		barrier();

		/* Atomically make the log entry valid */
		le->gen_id = cpu_to_le16(trans->gen_id);
		crfss_flush_buffer(le, LOGENTRY_SIZE, false);
		addr += le_size;
		le_start += le_size;
		le++;
	}
	return 0;
}


int crfss_commit_transaction(struct crfss_inode *ei, 
				crfss_transaction_t *trans)
{
	if (trans == NULL) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);
		return -EFAULT;
	}

	/* Add the commit log-entry */
	if(crfss_add_logentry(ei, trans, NULL, 0, LE_COMMIT)) {
		printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);
		return -EFAULT;
	}

#if defined(_DEVFS_DEBUG_RDWR)
	crfss_print_trans(ei, trans);
	crfss_print_journ(ei);
#endif
	//crfss_recover_journal(ei);

	free_transaction(ei, trans);
	return 0;
}

/*Create an inode transaction and add devfs inode info*/
crfss_transaction_t *crfss_new_ino_trans_log
	(struct crfss_inode *ei, struct inode *inode) {

	crfss_transaction_t *trans;

	if(unlikely(!ei->isjourn)) {
		 BUG_ON(!ei->isjourn);
	}

	if(unlikely(ei->cachep_init != CACHEP_INIT)){
		BUG_ON(ei->cachep_init != CACHEP_INIT);
	}

        trans = crfss_new_ino_trans (ei);
        if(!trans) {
		BUG_ON(!trans);
                goto ino_trans_err;
        }	

        if(crfss_add_logentry(ei, trans, inode, MAX_DATA_PER_LENTRY,
                        LE_DATA) ) {
		goto ino_trans_err;
	}
	return trans;
ino_trans_err:
	printk(KERN_ALERT "%s:%d Failed \n",__FUNCTION__, __LINE__);
	return NULL;
}

/*Alloc the per-file journal*/
crfss_journal_t *crfss_set_journal(struct crfss_inode *ei){

	if(!ei) {
		printk(KERN_ALERT "%s:%d Failed\n",__FUNCTION__,__LINE__);
		return NULL;
	}
	ei->journal = (crfss_journal_t *)vmalloc(FILEJOURNSZ);
	if(!ei->journal) {
		printk(KERN_ALERT "%s:%d Failed\n",__FUNCTION__,__LINE__);
		return NULL;
	}
	memset(ei->journal, 0, FILEJOURNSZ);

	ei->jsize = FILEJOURNSZ;	
	return ei->journal;	
}

/* Creates the per-inode journal. Assumes
* the caller passes a non-zero devfs inode
*/
int create_journal(struct crfss_inode *ei) {

	crfss_journal_t  *journ;
	uint64_t base;
	int retval = 0;

	ei->journal = NULL;
	ei->jsize = 0;

	journ = crfss_set_journal(ei);
	if(!journ) {
		printk(KERN_ALERT "%s:%d journal NULL \n",
		    __FUNCTION__,__LINE__);
		retval = -EFAULT;	
		return retval;	
	}
	base =  crfss_get_journal_base(ei);	
	if(!base) {
		printk(KERN_ALERT "%s:%d Failed\n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		return retval;
	}
	if(crfss_journal_hard_init(ei, base, FILEJOURNSZ)) {
		printk(KERN_ALERT "%s:%d crfss_journal_hard_init NULL \n",
			 __FUNCTION__,__LINE__);
		retval = -EFAULT;
		return retval;
	}
	return 0;
}

/*Caller takes the responsibility to check journal not created*/
int init_journal( struct inode *inode) {

	int retval = 0;
	struct crfss_inode *ei = NULL;	

	ei = DEVFS_I(inode);
	if(!ei) {
		printk(KERN_ALERT "%s:%d ei NULL \n", __FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_init_journ;
	}

		printk(KERN_ALERT "ei = %llx, ei->cachep_init = %d | %s:%d\n",
			ei, ei->cachep_init, __FUNCTION__, __LINE__);

	/* This condition should be false*/
	//if(unlikely(ei->isjourn && (ei->cachep_init != CACHEP_INIT))){
	if(unlikely(ei->isjourn && (ei->cachep_init == CACHEP_INIT))){
	      printk(KERN_ALERT "%s:%d Failed \n",
			      __FUNCTION__,__LINE__);

	      BUG_ON(ei->cachep_init == CACHEP_INIT);
	      goto err_init_journ;
	}

	if(init_crfss_transaction_cache(ei)) {
		printk(KERN_ALERT "%s:%d Failed\n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_init_journ;
	}

	if(create_journal(ei)) {
		printk(KERN_ALERT "%s:%d Failed journal\n",
			__FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_init_journ;
	}
	 	
	crfss_dbgv("%s:%d Success %lu\n",
		__FUNCTION__,__LINE__, inode->i_ino);

	ei->cachep_init = CACHEP_INIT;	
	ei->isjourn = 1;

	return retval;	

err_init_journ:   
	return retval;	
}


int free_journal( struct inode *inode) {

	int retval = 0;
	struct crfss_inode *ei = NULL;	


	crfss_dbgv("%s:%d Called\n", __FUNCTION__,__LINE__);

	ei = DEVFS_I(inode);
	if(!ei) {
		printk(KERN_ALERT "%s:%d ei NULL \n", __FUNCTION__,__LINE__);
		retval = -EFAULT;
		goto err_free_journ;
	}

	if(destroy_crfss_transaction_cache(ei)) {
		printk(KERN_ALERT "%s:%d Failed\n",__FUNCTION__,__LINE__);
		retval = -EFAULT;
		dump_stack();
		goto err_free_journ;
	}

	if(!ei->journal) {
		printk(KERN_ALERT "%s:%d Failed journal\n",
		 __FUNCTION__,__LINE__);
		goto err_free_journ;
	}
	vfree(ei->journal);	
	ei->journal = NULL;

	ei->cachep_init = 0;
	ei->isjourn = 0;

	//crfss_dbgv("%s:%d Success\n",__FUNCTION__,__LINE__);

	printk(KERN_ALERT "%s:%d Success \n",__FUNCTION__,__LINE__);

	
	return retval;	

err_free_journ:   
	return retval;	
}

inline void free_transaction(struct crfss_inode *ei, 
			crfss_transaction_t *trans)
{
	if(!ei->trans_cachep) {
		printk(KERN_ALERT "%s:%d Failed \n",
			__FUNCTION__,__LINE__);
	}
	kmem_cache_free(ei->trans_cachep, trans);
}


inline crfss_transaction_t *alloc_transaction
				(struct crfss_inode *ei)
{
	if (ei->cachep_init != CACHEP_INIT || ei->trans_cachep == NULL) {
		printk(KERN_ALERT "%s:%d crfss_alloc_transaction NULL \n",
		 __FUNCTION__,__LINE__);
		return NULL;
	}
	return (crfss_transaction_t *)
		kmem_cache_alloc(ei->trans_cachep, GFP_NOFS);
}


int init_crfss_transaction_cache(struct crfss_inode *ei)
{
	if(ei->cachep_init == CACHEP_INIT){
		printk(KERN_ALERT "%s:%d Reinit failed \n",
			__FUNCTION__,__LINE__);
		return 0;
	}	

	ei->trans_cachep = kmem_cache_create("crfss_journal_transaction",
			sizeof(crfss_transaction_t), 0, (SLAB_RECLAIM_ACCOUNT |
			SLAB_MEM_SPREAD), NULL);

	if (ei->trans_cachep == NULL) {
		printk(KERN_ALERT "%s:%d init_transaction_cache NULL \n",
		 __FUNCTION__,__LINE__);
		return -ENOMEM;
	}

	crfss_dbgv("%s:%d SUCCESS \n", __FUNCTION__,__LINE__);

	return 0;
}

int destroy_crfss_transaction_cache(struct crfss_inode *ei)
{
	if (ei->trans_cachep) {
		kmem_cache_destroy(ei->trans_cachep);
	}else {
		printk(KERN_ALERT "%s:%d \n", __FUNCTION__,__LINE__);
		return -EFAULT;	
	}
	ei->trans_cachep = NULL;
	ei->cachep_init = 0;
	printk(KERN_ALERT "%s:%d SUCCESS \n", __FUNCTION__,__LINE__);
	return 0;
}


/* TODO AFTER_DEADLINE - Passing super_block NULL 
   Super block is not required for per-inode logging 
*/
static void invalidate_remaining_journal(struct crfss_inode *ei,
        void *journal_vaddr, uint32_t jtail, uint32_t jsize)
{
        crfss_logentry_t *le = (crfss_logentry_t *)(journal_vaddr + jtail);
        void *start = le;

        crfss_memunlock_range(ei, start, jsize - jtail);

        while (jtail < jsize) {
                invalidate_gen_id(le);
                le++;
                jtail += LOGENTRY_SIZE;
        }

        crfss_memlock_range(ei, start, jsize - jtail);
}



/* TODO AFTER_DEADLINE - Passing super_block NULL
   Super block is not required for per-inode logging
*/
static void crfss_forward_journal(struct crfss_inode *ei, crfss_journal_t *journal)
{
        uint16_t gen_id = le16_to_cpu(journal->gen_id);

        /* handle gen_id wrap around */
        if (gen_id == MAX_GEN_ID) {
                invalidate_remaining_journal(ei, ei->journal_base_addr,
                        le32_to_cpu(journal->tail), ei->jsize);
        }

        DEVFS_PERSISTENT_MARK();
        gen_id = next_gen_id(gen_id);
        /* make all changes persistent before advancing gen_id and head */
        DEVFS_PERSISTENT_BARRIER();

        crfss_memunlock_range(ei, journal, sizeof(*journal));

        journal->gen_id = cpu_to_le16(gen_id);
        barrier();
        journal->head = journal->tail;

        crfss_memlock_range(ei, journal, sizeof(*journal));
        crfss_flush_buffer(journal, sizeof(*journal), false);
}


/* Undo a valid log entry */
static inline void crfss_undo_logentry(struct crfss_inode *ei,
        crfss_logentry_t *le)
{
        char *data;

        if (le->size > 0) {

                //data = crfs_get_block(sb, le64_to_cpu(le->addr_offset));
		data = ei->journal_base_addr + le->addr_offset;

                /* Undo changes by flushing the log entry to pmfs */
                crfss_memunlock_range(ei, data, le->size);
                memcpy(data, le->data, le->size);
                crfss_memlock_range(ei, data, le->size);
                crfss_flush_buffer(data, le->size, false);
        }
}

/* can be called during journal recovery or transaction abort */
/* We need to Undo in the reverse order */
static void crfss_undo_transaction(struct crfss_inode *ei,
                crfss_transaction_t *trans)
{
        crfss_logentry_t *le;
        int i;
        uint16_t gen_id = trans->gen_id;

        le = trans->start_addr + trans->num_used;
        le--;
        for (i = trans->num_used - 1; i >= 0; i--, le--) {
                if (gen_id == le16_to_cpu(le->gen_id))
                       crfss_undo_logentry(ei, le);
        }
}

/* can be called by either during log cleaning or during journal recovery */
static void crfss_invalidate_logentries(struct crfss_inode *ei,
                crfss_transaction_t *trans)
{
        crfss_logentry_t *le = trans->start_addr;
        int i;

        crfss_memunlock_range(ei, trans->start_addr,
                        trans->num_entries * LOGENTRY_SIZE);

        for (i = 0; i < trans->num_entries; i++) {
                invalidate_gen_id(le);
                if (le->type == LE_START) {
                        DEVFS_PERSISTENT_MARK();
                        DEVFS_PERSISTENT_BARRIER();
                }
                le++;
        }
        crfss_memlock_range(ei, trans->start_addr,
                        trans->num_entries * LOGENTRY_SIZE);
}


/* recover the transaction ending at a valid log entry *le */
/* called for Undo log and traverses the journal backward */
static uint32_t crfss_recover_transaction(struct crfss_inode *ei, uint32_t head,
		uint32_t tail, crfss_logentry_t *le)
{
	crfss_transaction_t trans;
	bool cmt_or_abrt_found = false, start_found = false;
	uint16_t gen_id = le16_to_cpu(le->gen_id);

	memset(&trans, 0, sizeof(trans));
	trans.transaction_id = le32_to_cpu(le->transaction_id);
	trans.gen_id = gen_id;

	do {
		trans.num_entries++;
		trans.num_used++;

		if (gen_id == le16_to_cpu(le->gen_id)) {
			/* Handle committed/aborted transactions */
			if (le->type & LE_COMMIT || le->type & LE_ABORT)
				cmt_or_abrt_found = true;
			if (le->type & LE_START) {
				trans.start_addr = le;
				start_found = true;
				break;
			}
		}
		if (tail == 0 || tail == head)
		    break;
		/* prev log entry */
		le--;
		/* Handle uncommitted transactions */
		if ((gen_id == le16_to_cpu(le->gen_id))
			&& (le->type & LE_COMMIT || le->type & LE_ABORT)) {
			BUG_ON(trans.transaction_id == 
				le32_to_cpu(le->transaction_id));
			le++;
			break;
		}
		tail = prev_log_entry(ei->jsize, tail);
	} while (1);

	if (start_found && !cmt_or_abrt_found)
		crfss_undo_transaction(ei, &trans);

	if (gen_id == MAX_GEN_ID) {
		if (!start_found)
			trans.start_addr = le;
		/* make sure the changes made by crfs_undo_transaction() are
		 * persistent before invalidating the log entries */
		if (start_found && !cmt_or_abrt_found) {
			DEVFS_PERSISTENT_MARK();
			DEVFS_PERSISTENT_BARRIER();
		}
		crfss_invalidate_logentries(ei, &trans);
	}
	return tail;
}


int crfss_recover_journal(struct crfss_inode *ei)
{
        crfss_journal_t *journal = crfss_get_journal(ei);
        uint32_t tail = le32_to_cpu(journal->tail);
        uint32_t head = le32_to_cpu(journal->head);
        uint16_t gen_id = le16_to_cpu(journal->gen_id);

        /* is the journal empty? true if unmounted properly. */
        if (head == tail)
                return 0;

        printk(KERN_ALERT "DEVFS: journal recovery. head:tail %u:%u gen_id %d\n",
                head, tail, gen_id);

#if defined(AFTER_DEADLINE)
        if (sbi->redo_log)
                crfs_recover_redo_journal(sb);
        else
#endif
        crfss_recover_undo_journal(ei);
        return 0;
}


int crfss_recover_undo_journal(struct crfss_inode *ei)
{
        crfss_journal_t  *journal = crfss_get_journal(ei);
        uint32_t tail = le32_to_cpu(journal->tail);
        uint32_t head = le32_to_cpu(journal->head);
        uint16_t gen_id = le16_to_cpu(journal->gen_id);
        crfss_logentry_t *le;

	printk(KERN_ALERT "%s:%d Enter head %u, tail %u gen_id %d\n",
			 __FUNCTION__,__LINE__, head, tail, gen_id);

        while (head != tail) {

                /* handle journal wraparound */
                if (tail == 0)
                        gen_id = prev_gen_id(gen_id);

                tail = prev_log_entry(ei->jsize, tail);

                le = (crfss_logentry_t *)(ei->journal_base_addr + tail);

		if(le) {
			crfss_print_logentry(le);
		}
	
                if (gen_id == le16_to_cpu(le->gen_id)) {
                        tail = crfss_recover_transaction(ei, head, tail, le);
                } else {
                        if (gen_id == MAX_GEN_ID) {
                                crfss_memunlock_range(ei, le, sizeof(*le));
                                invalidate_gen_id(le);
                                crfss_memlock_range(ei, le, sizeof(*le));
		
				printk(KERN_ALERT "%s:%d gen_id == MAX_GEN_ID \n",
					__FUNCTION__,__LINE__);
                        }
                }
        }

	/* TODO AFTER_DEADLINE - Passing super_block NULL
	   Super block is not required for per-inode logging
	*/
        crfss_forward_journal(ei, journal);
        DEVFS_PERSISTENT_MARK();
        DEVFS_PERSISTENT_BARRIER();
        return 0;
}

