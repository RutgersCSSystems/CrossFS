/*
 * PMFS emulated persistence. This file contains code to 
 * handle data blocks of various sizes efficiently.
 *
 * Persistent Memory File System
 * Copyright (c) 2012-2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/bitops.h>
#include "pmfs.h"

static struct kmem_cache *crfs_blocknode_cachep;


#if 1
//int __init crfss_init_blocknode_cache(void)
int crfss_init_blocknode_cache(void)
{

	printk(KERN_ALERT "init_blocknode_cache start %s:%d \n",
		__FUNCTION__,__LINE__);

#if 1
        crfs_blocknode_cachep = kmem_cache_create("crfs_blocknode_cache",
                                        sizeof(struct crfs_blocknode),
                                        0, (SLAB_RECLAIM_ACCOUNT |
                                        SLAB_MEM_SPREAD), NULL);
        if (crfs_blocknode_cachep == NULL) {
		printk(KERN_ALERT "%s:%d init_blocknode_cache failed \n",
			__FUNCTION__,__LINE__);
                return -ENOMEM;
	}
#endif
        printk(KERN_ALERT "init_blocknode_cache end %s:%d \n",
                __FUNCTION__,__LINE__);

        return 0;
}
#endif

struct crfs_blocknode *crfs_alloc_blocknode(struct super_block *sb)
{
        struct crfs_blocknode *p;

        //struct crfs_sb_info *sbi = PMFS_SB(sb);
	struct crfss_sb_info *sbi = DEVFS_SB(sb);

        p = (struct crfs_blocknode *)
                kmem_cache_alloc(crfs_blocknode_cachep, GFP_NOFS);
        if (p) {
                sbi->num_blocknode_allocated++;
        }
        return p;
}


void crfs_init_blockmap(struct super_block *sb, unsigned long init_used_size)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	//struct crfss_sb_info *sbi = DEVFS_SB(sb);

	unsigned long num_used_block;
	struct crfs_blocknode *blknode;

        printk(KERN_ALERT "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);

	if(!sbi) {
		printk(KERN_ALERT "%s:%d crfs_init_blockmap failed\n",
				__FUNCTION__,__LINE__);
	}
	printk(KERN_ALERT "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);

	 printk(KERN_ALERT "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);

#if 1
	if(crfss_init_blocknode_cache()){
		printk(KERN_ALERT "%s:%d crfs_init_blockmap failed \n",
			        __FUNCTION__,__LINE__);
		return;
	}
#endif

	printk(KERN_ALERT "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);

	num_used_block = (init_used_size + sb->s_blocksize - 1) >>
		sb->s_blocksize_bits;

	printk(KERN_ALERT "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);

	blknode = crfs_alloc_blocknode(sb);

	printk(KERN_ALERT "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);

	if (blknode == NULL) {

		//DEVFS_ASSERT(0);
		printk(KERN_ALERT "%s:%d crfs_init_blockmap failed \n",
			__FUNCTION__,__LINE__);
		return;
	}
	blknode->block_low = sbi->block_start;
	blknode->block_high = sbi->block_start + num_used_block - 1;
	sbi->num_free_blocks -= num_used_block;
	list_add(&blknode->link, &sbi->block_inuse_head);


	crfss_dbgv("Success %s:%d block_low %lu block_high %lu num_free_blocks %lu\n", 
		__FUNCTION__,__LINE__, blknode->block_low, blknode->block_high, 
		sbi->num_free_blocks);
}



static struct crfs_blocknode *crfs_next_blocknode(struct crfs_blocknode *i,
						  struct list_head *head)
{
	if (list_is_last(&i->link, head))
		return NULL;
	return list_first_entry(&i->link, typeof(*i), link);
}



/* Caller must hold the super_block lock.  If start_hint is provided, it is
 * only valid until the caller releases the super_block lock. */
void __crfs_free_block(struct super_block *sb, unsigned long blocknr,
		      unsigned short btype, struct crfs_blocknode **start_hint)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	//struct crfss_sb_info *sbi = DEVFS_SB(sb);

	struct list_head *head = &(sbi->block_inuse_head);
	unsigned long new_block_low;
	unsigned long new_block_high;
	unsigned long num_blocks = 0;
	struct crfs_blocknode *i;
	struct crfs_blocknode *free_blocknode= NULL;
	struct crfs_blocknode *curr_node;

	num_blocks = crfs_get_numblocks(btype);
	new_block_low = blocknr;
	new_block_high = blocknr + num_blocks - 1;

	BUG_ON(list_empty(head));

	if (start_hint && *start_hint &&
	    new_block_low >= (*start_hint)->block_low)
		i = *start_hint;
	else
		i = list_first_entry(head, typeof(*i), link);

	list_for_each_entry_from(i, head, link) {

		if (new_block_low > i->block_high) {
			/* skip to next blocknode */
			continue;
		}

		if ((new_block_low == i->block_low) &&
			(new_block_high == i->block_high)) {
			/* fits entire datablock */
			if (start_hint)
				*start_hint = crfs_next_blocknode(i, head);
			list_del(&i->link);
			free_blocknode = i;
			sbi->num_blocknode_allocated--;
			sbi->num_free_blocks += num_blocks;
			goto block_found;
		}
		if ((new_block_low == i->block_low) &&
			(new_block_high < i->block_high)) {
			/* Aligns left */
			i->block_low = new_block_high + 1;
			sbi->num_free_blocks += num_blocks;
			if (start_hint)
				*start_hint = i;
			goto block_found;
		}
		if ((new_block_low > i->block_low) && 
			(new_block_high == i->block_high)) {
			/* Aligns right */
			i->block_high = new_block_low - 1;
			sbi->num_free_blocks += num_blocks;
			if (start_hint)
				*start_hint = crfs_next_blocknode(i, head);
			goto block_found;
		}
		if ((new_block_low > i->block_low) &&
			(new_block_high < i->block_high)) {
			/* Aligns somewhere in the middle */
			curr_node = crfs_alloc_blocknode(sb);
			DEVFS_ASSERT(curr_node);
			if (curr_node == NULL) {
				/* returning without freeing the block*/
				goto block_found;
			}
			curr_node->block_low = new_block_high + 1;
			curr_node->block_high = i->block_high;
			i->block_high = new_block_low - 1;
			list_add(&curr_node->link, &i->link);
			sbi->num_free_blocks += num_blocks;
			if (start_hint)
				*start_hint = curr_node;
			goto block_found;
		}
	}

	//crfs_error_mng(sb, "Unable to free block %ld\n", blocknr);
	printk(KERN_ALERT "Unable to free block %ld\n", blocknr);

block_found:

	if (free_blocknode)
		__crfs_free_blocknode(free_blocknode);
}

void crfs_free_block(struct super_block *sb, unsigned long blocknr,
		      unsigned short btype)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	//struct crfss_sb_info *sbi = DEVFS_SB(sb);
	mutex_lock(&sbi->s_lock);
	__crfs_free_block(sb, blocknr, btype, NULL);
	mutex_unlock(&sbi->s_lock);
}

int crfs_new_block(struct super_block *sb, unsigned long *blocknr,
	unsigned short btype, int zero)
{


	struct crfs_sb_info *sbi = PMFS_SB(sb);
	//struct crfss_sb_info *sbi = DEVFS_SB(sb);


	struct list_head *head = &(sbi->block_inuse_head);
	struct crfs_blocknode *i, *next_i;
	struct crfs_blocknode *free_blocknode= NULL;
	void *bp;
	unsigned long num_blocks = 0;
	struct crfs_blocknode *curr_node;
	int errval = 0;
	bool found = 0;
	unsigned long next_block_low;
	unsigned long new_block_low;
	unsigned long new_block_high;


	/*get the type of block*/
	num_blocks = crfs_get_numblocks(btype);

	mutex_lock(&sbi->s_lock);

	list_for_each_entry(i, head, link) {
		if (i->link.next == head) {
			next_i = NULL;
			next_block_low = sbi->block_end;
		} else {
			next_i = list_entry(i->link.next, typeof(*i), link);
			next_block_low = next_i->block_low;
		}

		new_block_low = (i->block_high + num_blocks) & ~(num_blocks - 1);
		new_block_high = new_block_low + num_blocks - 1;

		if (new_block_high >= next_block_low) {
			/* Does not fit - skip to next blocknode */
			continue;
		}

		if ((new_block_low == (i->block_high + 1)) &&
			(new_block_high == (next_block_low - 1)))
		{
			/* Fill the gap completely */
			if (next_i) {
				i->block_high = next_i->block_high;
				list_del(&next_i->link);
				free_blocknode = next_i;
				sbi->num_blocknode_allocated--;
			} else {
				i->block_high = new_block_high;
			}
			found = 1;
			break;
		}

		if ((new_block_low == (i->block_high + 1)) &&
			(new_block_high < (next_block_low - 1))) {
			/* Aligns to left */
			i->block_high = new_block_high;
			found = 1;
			break;
		}

		if ((new_block_low > (i->block_high + 1)) &&
			(new_block_high == (next_block_low - 1))) {
			/* Aligns to right */
			if (next_i) {
				/* right node exist */
				next_i->block_low = new_block_low;
			} else {
				/* right node does NOT exist */
				curr_node = crfs_alloc_blocknode(sb);
				PMFS_ASSERT(curr_node);
				if (curr_node == NULL) {
					errval = -ENOSPC;
					break;
				}
				curr_node->block_low = new_block_low;
				curr_node->block_high = new_block_high;
				list_add(&curr_node->link, &i->link);
			}
			found = 1;
			break;
		}

		if ((new_block_low > (i->block_high + 1)) &&
			(new_block_high < (next_block_low - 1))) {
			/* Aligns somewhere in the middle */
			curr_node = crfs_alloc_blocknode(sb);
			PMFS_ASSERT(curr_node);
			if (curr_node == NULL) {
				errval = -ENOSPC;
				break;
			}
			curr_node->block_low = new_block_low;
			curr_node->block_high = new_block_high;
			list_add(&curr_node->link, &i->link);
			found = 1;
			break;
		}
	}
	
	if (found == 1) {
		sbi->num_free_blocks -= num_blocks;
	}	

	mutex_unlock(&sbi->s_lock);

	if (free_blocknode)
		__crfs_free_blocknode(free_blocknode);

	if (found == 0) {
		printk(KERN_ALERT "Unable to alloc blocknr %ld\n",*blocknr);
		return -ENOSPC;
	}

	if (zero) {
		size_t size;
		bp = crfs_get_block(sb, crfs_get_block_off(sb, new_block_low, btype));
		crfs_memunlock_block(sb, bp); //TBDTBD: Need to fix this
		if (btype == PMFS_BLOCK_TYPE_4K)
			size = 0x1 << 12;
		else if (btype == PMFS_BLOCK_TYPE_2M)
			size = 0x1 << 21;
		else
			size = 0x1 << 30;
		memset_nt(bp, 0, size);
		crfs_memlock_block(sb, bp);
	}
	*blocknr = new_block_low;

	 crfss_dbgv("Success %s:%d block_low %lu block_high %lu num_free_blocks %lu "
			"*blocknr %lu\n",
        	 __FUNCTION__,__LINE__, curr_node->block_low, curr_node->block_high,
	         sbi->num_free_blocks, *blocknr);


	return errval;
}

unsigned long crfs_count_free_blocks(struct super_block *sb)
{
	//struct crfs_sb_info *sbi = PMFS_SB(sb);
	struct crfss_sb_info *sbi = DEVFS_SB(sb);
	return sbi->num_free_blocks; 
}
