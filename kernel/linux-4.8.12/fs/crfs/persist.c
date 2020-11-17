/*
 * PMFS emulated persistence. this file contains code to load pmfs from a
 * file into memory and store pmfs to a file from memory.
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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mount.h>
#include <linux/mm.h>
#include <linux/bitops.h>
#include <linux/cred.h>
#include <linux/backing-dev.h>
#include "pmfs.h"

static ssize_t crfs_write_backing_store(struct file *flp, char *src,
		ssize_t bytes, loff_t *woff)
{
	mm_segment_t old_fs;
	ssize_t len = 0;

	if (bytes > 0) {
		old_fs = get_fs();
		set_fs(get_ds());
		len = vfs_write(flp, src, bytes, woff);
		set_fs(old_fs);
		if (len <= 0)
			crfs_dbg_verbose("Could not write file or corrupted pmfs\n");
	}
	return len;
}

static ssize_t crfs_read_backing_store(struct file *flp, char *dest,
	ssize_t bytes, loff_t *roff)
{
	mm_segment_t old_fs;
	ssize_t len = 0;

	if (bytes > 0) {
		old_fs = get_fs();
		set_fs(get_ds());
		len = vfs_read(flp, dest, bytes, roff);
		set_fs(old_fs);
		if (len <= 0)
			crfs_dbg_verbose("Could not read file or corrupted pmfs\n");
	}
	return len;
}

/* Stores PMFS memory image into a storage file. Uses the allocation blocknode
 * linked list to determine which memory ranges to save */
static int crfs_storefs(struct file *flp, struct super_block *sb)
{
	loff_t woff = 0;
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	u64 num_blocknodes = sbi->num_blocknode_allocated, size;
	struct list_head *head = &(sbi->block_inuse_head);
	struct crfs_blocknode *i;
	struct crfs_blocknode_lowhigh p;
	char *ptr;

	crfs_info("storing pmfs to %s with 0x%llx blknodes\n",
			   sbi->crfs_backing_file, num_blocknodes);
	/* first save the number of blocknodes */
	if (crfs_write_backing_store(flp, (char *)&num_blocknodes, sizeof(u64),
		    &woff) != sizeof(u64))
		goto out;
	/* Then save the blocks containing blocknodes. */
	list_for_each_entry(i, head, link) {
		p.block_low = cpu_to_le64(i->block_low);
		p.block_high = cpu_to_le64(i->block_high);
		if (crfs_write_backing_store(flp, (char *)&p, sizeof(p), &woff)
				!= sizeof(p))
			goto out;
	}
	/* align the write offset on 4K boundary */
	woff = (woff + PAGE_SIZE - 1) & ~(0xFFFUL);
	/* Now save all the memory ranges allocated in the PMFS. These ranges
	 * are specified by the block_low and block_high fields of every
	 * struct crfs_blocknode_lowhigh */
	list_for_each_entry(i, head, link) {
		if (i->block_low == 0)
			ptr = (char *)crfs_get_super(sb);
		else
			ptr = crfs_get_block(sb, i->block_low << PAGE_SHIFT);
		size = (i->block_high - i->block_low + 1) << PAGE_SHIFT;
		if (crfs_write_backing_store(flp, ptr, size, &woff) != size)
			goto out;
	}
	vfs_fsync(flp, 0);
	return 0;
out:
	return -EINVAL;
}

static int crfs_loadfs(struct file *flp, struct super_block *sb)
{
	char *crfs_base, *buf1, *buf2, *ptr;
	struct crfs_super_block *super;
	loff_t roff = 0;
	int retval = -EINVAL;
	u64 crfs_size, buf_sz, num_blocknodes, i, size; 
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	struct crfs_blocknode_lowhigh *p;

	if (crfs_read_backing_store(flp, (char *)&num_blocknodes, sizeof(u64),
		    &roff) != sizeof(u64))
		return retval;

	crfs_info("Loading PMFS from %s to phys %llx with 0x%llx blknodes\n",
		sbi->crfs_backing_file, sbi->phys_addr, num_blocknodes);
	buf_sz = num_blocknodes * sizeof(struct crfs_blocknode_lowhigh);

	buf1 = kmalloc(buf_sz, GFP_KERNEL);
	if (buf1 == NULL)
		return retval;

	if (crfs_read_backing_store(flp, buf1, buf_sz, &roff) != buf_sz)
		goto out1;
	p = (struct crfs_blocknode_lowhigh *)buf1;

	/* align the read offset on 4K boundary */
	roff = (roff + PAGE_SIZE - 1) & ~(0xFFFUL);

	buf2 = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (buf2 == NULL)
		goto out1;
	if (crfs_read_backing_store(flp, buf2, PAGE_SIZE, &roff) != PAGE_SIZE)
		goto out2;

	super = (struct crfs_super_block *)buf2;
	if (crfs_check_integrity(NULL, super) == 0) {
		crfs_err(sb, "file contains invalid pmfs\n");
		goto out2;
	}
	crfs_size = le64_to_cpu(super->s_size);
	crfs_base = crfs_ioremap(NULL, sbi->phys_addr, crfs_size);
	if (!crfs_base) {
		crfs_err(sb, "ioremap of the pmfs image failed\n");
		goto out2;
	}
	memcpy(crfs_base, buf2, PAGE_SIZE);
	/* now walk through the blocknode list and copy every range specified
	 * in the list to PMFS area */
	for (i = 0; i < num_blocknodes; i++, p++) {
		if (p->block_low == 0) {
			ptr = crfs_base + PAGE_SIZE;
			size = (le64_to_cpu(p->block_high) - 
				le64_to_cpu(p->block_low)) << PAGE_SHIFT;
		} else {
			ptr = crfs_base + (le64_to_cpu(p->block_low) << 
				PAGE_SHIFT);
			size = (le64_to_cpu(p->block_high) - 
				le64_to_cpu(p->block_low) + 1) << PAGE_SHIFT;
		}
		if (crfs_read_backing_store(flp, ptr, size, &roff) != size)
			goto out;
	}
	retval = 0;
out:
	iounmap(crfs_base);
	release_mem_region(sbi->phys_addr, crfs_size);
out2:
	kfree(buf2);
out1:
	kfree(buf1);
	return retval;
}

void crfs_load_from_file(struct super_block *sb)
{
	struct file *flp;
	mm_segment_t oldfs;
	struct crfs_sb_info *sbi = PMFS_SB(sb);

	if (strlen(sbi->crfs_backing_file) && sbi->crfs_backing_option != 1) {
		oldfs = get_fs();
		set_fs(get_ds());
		flp = filp_open(sbi->crfs_backing_file, O_RDONLY | O_LARGEFILE,
			S_IRWXU);
		set_fs(oldfs);
		if (IS_ERR(flp)) {
			crfs_info("Can't open backing file %s\n",
				   sbi->crfs_backing_file);
		} else {
			crfs_loadfs(flp, sb);
			oldfs = get_fs();
			set_fs(get_ds());
			filp_close(flp, current->files);
			set_fs(oldfs);
		}
	}
}

void crfs_store_to_file(struct super_block *sb)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	if (strlen(sbi->crfs_backing_file) && sbi->crfs_backing_option != 2) {
		struct file *flp;
		mm_segment_t oldfs;
		oldfs = get_fs();
		set_fs(get_ds());
		flp = filp_open(sbi->crfs_backing_file,
			O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, S_IRWXU);
		set_fs(oldfs);
		if (IS_ERR(flp)) {
			crfs_info("Can't open file %s\n",
				   sbi->crfs_backing_file);
		} else {
			crfs_storefs(flp, sb);
			oldfs = get_fs();
			set_fs(get_ds());
			filp_close(flp, current->files);
			set_fs(oldfs);
		}
	}
	sbi->crfs_backing_file[0] = '\0';
	sbi->crfs_backing_option = 0;
}
