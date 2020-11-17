/*
 * BRIEF DESCRIPTION
 *
 * Inode operations for directories.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/fs.h>
#include <linux/pagemap.h>
#include "pmfs.h"
#include "xip.h"

/*
 * Couple of helper functions - make the code slightly cleaner.
 */
static inline void crfs_inc_count(struct inode *inode, struct crfs_inode *pi)
{
	inc_nlink(inode);
	crfs_update_nlink(inode, pi);
}

static inline void crfs_dec_count(struct inode *inode, struct crfs_inode *pi)
{
	if (inode->i_nlink) {
		drop_nlink(inode);
		crfs_update_nlink(inode, pi);
	}
}

static inline int crfs_add_nondir(crfs_transaction_t *trans,
		struct inode *dir, struct dentry *dentry, struct inode *inode)
{
	struct crfs_inode *pi;
	int err = crfs_add_entry(trans, dentry, inode);

	if (!err) {
		d_instantiate(dentry, inode);
		unlock_new_inode(inode);
		return 0;
	}
	pi = crfs_get_inode(inode->i_sb, inode->i_ino);
	crfs_dec_count(inode, pi);
	unlock_new_inode(inode);
	iput(inode);
	return err;
}

static inline struct crfs_direntry *crfs_next_entry(struct crfs_direntry *p)
{
	return (struct crfs_direntry *)((char *)p + le16_to_cpu(p->de_len));
}

/*
 * Methods themselves.
 */
int crfs_check_dir_entry(const char *function, struct inode *dir,
			  struct crfs_direntry *de, u8 *base,
			  unsigned long offset)
{
	const char *error_msg = NULL;
	const int rlen = le16_to_cpu(de->de_len);

	if (unlikely(rlen < PMFS_DIR_REC_LEN(1)))
		error_msg = "de_len is smaller than minimal";
	else if (unlikely(rlen % 4 != 0))
		error_msg = "de_len % 4 != 0";
	else if (unlikely(rlen < PMFS_DIR_REC_LEN(de->name_len)))
		error_msg = "de_len is too small for name_len";
	else if (unlikely((((u8 *)de - base) + rlen > dir->i_sb->s_blocksize)))
		error_msg = "directory entry across blocks";

	if (unlikely(error_msg != NULL)) {
		crfs_dbg("bad entry in directory #%lu: %s - "
			  "offset=%lu, inode=%lu, rec_len=%d, name_len=%d",
			  dir->i_ino, error_msg, offset,
			  (unsigned long)le64_to_cpu(de->ino), rlen,
			  de->name_len);
	}

	return error_msg == NULL ? 1 : 0;
}

/*
 * Returns 0 if not found, -1 on failure, and 1 on success
 */
int crfs_search_dirblock(u8 *blk_base, struct inode *dir, struct qstr *child,
			  unsigned long	offset,
			  struct crfs_direntry **res_dir,
			  struct crfs_direntry **prev_dir)
{
	struct crfs_direntry *de;
	struct crfs_direntry *pde = NULL;
	char *dlimit;
	int de_len;
	const char *name = child->name;
	int namelen = child->len;

	de = (struct crfs_direntry *)blk_base;
	dlimit = blk_base + dir->i_sb->s_blocksize;
	while ((char *)de < dlimit) {
		/* this code is executed quadratically often */
		/* do minimal checking `by hand' */

		if ((char *)de + namelen <= dlimit &&
		    crfs_match(namelen, name, de)) {
			/* found a match - just to be sure, do a full check */
			if (!crfs_check_dir_entry("crfs_inode_by_name",
						   dir, de, blk_base, offset))
				return -1;
			*res_dir = de;
			if (prev_dir)
				*prev_dir = pde;
			return 1;
		}
		/* prevent looping on a bad block */
		de_len = le16_to_cpu(de->de_len);
		if (de_len <= 0)
			return -1;
		offset += de_len;
		pde = de;
		de = (struct crfs_direntry *)((char *)de + de_len);
	}
	return 0;
}

static ino_t crfs_inode_by_name(struct inode *dir, struct qstr *entry,
				 struct crfs_direntry **res_entry)
{
	struct crfs_inode *pi;
	ino_t i_no = 0;
	int namelen, nblocks, i;
	u8 *blk_base;
	const u8 *name = entry->name;
	struct super_block *sb = dir->i_sb;
	unsigned long block, start;
	struct crfs_inode_vfs *si = PMFS_I(dir);

	pi = crfs_get_inode(sb, dir->i_ino);

	namelen = entry->len;
	if (namelen > PMFS_NAME_LEN)
		return 0;
	if ((namelen <= 2) && (name[0] == '.') &&
	    (name[1] == '.' || name[1] == 0)) {
		/*
		 * "." or ".." will only be in the first block
		 */
		block = start = 0;
		nblocks = 1;
		goto restart;
	}
	nblocks = dir->i_size >> dir->i_sb->s_blocksize_bits;
	start = si->i_dir_start_lookup;
	if (start >= nblocks)
		start = 0;
	block = start;
restart:
	do {
		blk_base =
			crfs_get_block(sb, crfs_find_data_block(dir, block));
		if (!blk_base)
			goto done;
		i = crfs_search_dirblock(blk_base, dir, entry,
					  block << sb->s_blocksize_bits,
					  res_entry, NULL);
		if (i == 1) {
			si->i_dir_start_lookup = block;
			i_no = le64_to_cpu((*res_entry)->ino);
			goto done;
		} else {
			if (i < 0)
				goto done;
		}
		if (++block >= nblocks)
			block = 0;
	} while (block != start);
	/*
	 * If the directory has grown while we were searching, then
	 * search the last part of the directory before giving up.
	 */
	block = nblocks;
	nblocks = dir->i_size >> sb->s_blocksize_bits;
	if (block < nblocks) {
		start = 0;
		goto restart;
	}
done:
	return i_no;
}

static struct dentry *crfs_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags)
{
	struct inode *inode = NULL;
	struct crfs_direntry *de;
	ino_t ino;

	if (dentry->d_name.len > PMFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	ino = crfs_inode_by_name(dir, &dentry->d_name, &de);
	if (ino) {
		inode = crfs_iget(dir->i_sb, ino);
		if (inode == ERR_PTR(-ESTALE)) {
			crfs_err(dir->i_sb, __func__,
				  "deleted inode referenced: %lu",
				  (unsigned long)ino);
			return ERR_PTR(-EIO);
		}
	}

	//printk(KERN_ALERT "ino = %d, name = %s | %s:%d\n", ino, dentry->d_name.name, __FUNCTION__, __LINE__);

	return d_splice_alias(inode, dentry);
}

/*
 * By the time this is called, we already have created
 * the directory cache entry for the new file, but it
 * is so far negative - it has no inode.
 *
 * If the create succeeds, we fill in the inode information
 * with d_instantiate().
 */
static int crfs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			bool excl)
{
	struct inode *inode = NULL;
	int err = PTR_ERR(inode);
	struct super_block *sb = dir->i_sb;
	crfs_transaction_t *trans;

	/* two log entries for new inode, 1 lentry for dir inode, 1 for dir
	 * inode's b-tree, 2 lentries for logging dir entry
	 */
	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
		MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}

	inode = crfs_new_inode(trans, dir, mode, &dentry->d_name);
	if (IS_ERR(inode))
		goto out_err;
	inode->i_op = &crfs_file_inode_operations;
	inode->i_mapping->a_ops = &crfs_aops_xip;
	inode->i_fop = &crfs_xip_file_operations;
	err = crfs_add_nondir(trans, dir, dentry, inode);
	if (err)
		goto out_err;
	crfs_commit_transaction(sb, trans);

	//printk(KERN_ALERT "inode = %llx, name = %s | %s:%d\n", inode, dentry->d_name.name, __FUNCTION__, __LINE__);

out:
	return err;
out_err:
	crfs_abort_transaction(sb, trans);
	return err;
}

static int crfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
		       dev_t rdev)
{
	struct inode *inode = NULL;
	int err = PTR_ERR(inode);
	crfs_transaction_t *trans;
	struct super_block *sb = dir->i_sb;
	struct crfs_inode *pi;

	/* 2 log entries for new inode, 1 lentry for dir inode, 1 for dir
	 * inode's b-tree, 2 lentries for logging dir entry
	 */
	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
			MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}

	inode = crfs_new_inode(trans, dir, mode, &dentry->d_name);
	if (!IS_ERR(inode))
		goto out_err;
	init_special_inode(inode, mode, rdev);
	inode->i_op = &crfs_special_inode_operations;

	pi = crfs_get_inode(sb, inode->i_ino);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		pi->dev.rdev = cpu_to_le32(inode->i_rdev);
	err = crfs_add_nondir(trans, dir, dentry, inode);
	if (err)
		goto out_err;
	crfs_commit_transaction(sb, trans);
out:
	return err;
out_err:
	crfs_abort_transaction(sb, trans);
	return err;
}

static int crfs_symlink(struct inode *dir, struct dentry *dentry,
			 const char *symname)
{
	struct super_block *sb = dir->i_sb;
	int err = -ENAMETOOLONG;
	unsigned len = strlen(symname);
	struct inode *inode;
	crfs_transaction_t *trans;
	struct crfs_inode *pi;

	if (len + 1 > sb->s_blocksize)
		goto out;

	/* 2 log entries for new inode, 1 lentry for dir inode, 1 for dir
	 * inode's b-tree, 2 lentries for logging dir entry
	 */
	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
			MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}

	inode = crfs_new_inode(trans, dir, S_IFLNK|S_IRWXUGO, &dentry->d_name);
	err = PTR_ERR(inode);
	if (IS_ERR(inode)) {
		crfs_abort_transaction(sb, trans);
		goto out;
	}

	inode->i_op = &crfs_symlink_inode_operations;
	inode->i_mapping->a_ops = &crfs_aops_xip;

	pi = crfs_get_inode(sb, inode->i_ino);
	err = crfs_block_symlink(inode, symname, len);
	if (err)
		goto out_fail;

	inode->i_size = len;
	crfs_update_isize(inode, pi);

	err = crfs_add_nondir(trans, dir, dentry, inode);
	if (err) {
		/* free up the allocated block to the symlink inode */
		crfs_setsize(inode, 0);
		crfs_abort_transaction(sb, trans);
		goto out;
	}

	crfs_commit_transaction(sb, trans);
out:
	return err;

out_fail:
	crfs_dec_count(inode, pi);
	unlock_new_inode(inode);
	iput(inode);
	crfs_abort_transaction(sb, trans);
	goto out;
}

static int crfs_link(struct dentry *dest_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct inode *inode = dest_dentry->d_inode;
	int err = -ENOMEM;
	crfs_transaction_t *trans;
	struct super_block *sb = inode->i_sb;
	struct crfs_inode *pi = crfs_get_inode(sb, inode->i_ino);

	if (inode->i_nlink >= PMFS_LINK_MAX)
		return -EMLINK;

	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
			MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}
	/* only need to log the first 48 bytes since we only modify ctime and
	 * i_links_count in this system call */
	crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	ihold(inode);

	err = crfs_add_entry(trans, dentry, inode);
	if (!err) {
		inode->i_ctime = CURRENT_TIME_SEC;
		inc_nlink(inode);

		crfs_memunlock_inode(sb, pi);
		pi->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
		pi->i_links_count = cpu_to_le16(inode->i_nlink);
		crfs_memlock_inode(sb, pi);

		d_instantiate(dentry, inode);
		crfs_commit_transaction(sb, trans);
	} else {
		iput(inode);
		crfs_abort_transaction(sb, trans);
	}
out:
	return err;
}

int crfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int retval = -ENOMEM;
	crfs_transaction_t *trans;
	struct super_block *sb = inode->i_sb;
	struct crfs_inode *pi = crfs_get_inode(sb, inode->i_ino);

	/*if(dir->isdevfs)
	     printk(KERN_ALERT "DEBUG: %s:%d inode = %llx\n",
		__FUNCTION__, __LINE__, dir);*/

	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
		MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		retval = PTR_ERR(trans);
		goto out;
	}
	crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	retval = crfs_remove_entry(trans, dentry, inode);
	if (retval)
		goto end_unlink;

	if (inode->i_nlink == 1)
		crfs_truncate_add(inode, inode->i_size);
	inode->i_ctime = dir->i_ctime;

	crfs_memunlock_inode(sb, pi);
	if (inode->i_nlink) {
		drop_nlink(inode);
		pi->i_links_count = cpu_to_le16(inode->i_nlink);
	}
	pi->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
	crfs_memlock_inode(sb, pi);

	crfs_commit_transaction(sb, trans);
	return 0;
end_unlink:
	crfs_abort_transaction(sb, trans);
out:
	return retval;
}

static int crfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	struct crfs_inode *pi, *pidir;
	struct crfs_direntry *de = NULL;
	struct super_block *sb = dir->i_sb;
	crfs_transaction_t *trans;
	int err = -EMLINK;
	char *blk_base;

	if (dir->i_nlink >= PMFS_LINK_MAX)
		goto out;

	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
			MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}

	inode = crfs_new_inode(trans, dir, S_IFDIR | mode, &dentry->d_name);
	err = PTR_ERR(inode);
	if (IS_ERR(inode)) {
		crfs_abort_transaction(sb, trans);
		goto out;
	}

	inode->i_op = &crfs_dir_inode_operations;
	inode->i_fop = &crfs_dir_operations;
	inode->i_mapping->a_ops = &crfs_aops_xip;

	/* since this is a new inode so we don't need to include this
	 * crfs_alloc_blocks in the transaction
	 */
	err = crfs_alloc_blocks(NULL, inode, 0, 1, false);
	if (err)
		goto out_clear_inode;
	inode->i_size = sb->s_blocksize;

	blk_base = crfs_get_block(sb, crfs_find_data_block(inode, 0));
	de = (struct crfs_direntry *)blk_base;
	crfs_memunlock_range(sb, blk_base, sb->s_blocksize);
	de->ino = cpu_to_le64(inode->i_ino);
	de->name_len = 1;
	de->de_len = cpu_to_le16(PMFS_DIR_REC_LEN(de->name_len));
	strcpy(de->name, ".");
	/*de->file_type = S_IFDIR; */
	de = crfs_next_entry(de);
	de->ino = cpu_to_le64(dir->i_ino);
	de->de_len = cpu_to_le16(sb->s_blocksize - PMFS_DIR_REC_LEN(1));
	de->name_len = 2;
	strcpy(de->name, "..");
	/*de->file_type =  S_IFDIR; */
	crfs_memlock_range(sb, blk_base, sb->s_blocksize);

	/* No need to journal the dir entries but we need to persist them */
	crfs_flush_buffer(blk_base, PMFS_DIR_REC_LEN(1) +
			PMFS_DIR_REC_LEN(2), true);

	set_nlink(inode, 2);

	err = crfs_add_entry(trans, dentry, inode);
	if (err) {
		crfs_dbg_verbose("failed to add dir entry\n");
		goto out_clear_inode;
	}
	pi = crfs_get_inode(sb, inode->i_ino);
	crfs_memunlock_inode(sb, pi);
	pi->i_links_count = cpu_to_le16(inode->i_nlink);
	pi->i_size = cpu_to_le64(inode->i_size);
	crfs_memlock_inode(sb, pi);

	pidir = crfs_get_inode(sb, dir->i_ino);
	crfs_inc_count(dir, pidir);
	d_instantiate(dentry, inode);
	unlock_new_inode(inode);

	crfs_commit_transaction(sb, trans);

out:
	return err;

out_clear_inode:
	clear_nlink(inode);
	unlock_new_inode(inode);
	iput(inode);
	crfs_abort_transaction(sb, trans);
	goto out;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
static int crfs_empty_dir(struct inode *inode)
{
	unsigned long offset;
	struct crfs_direntry *de, *de1;
	struct super_block *sb;
	char *blk_base;
	int err = 0;

	sb = inode->i_sb;
	if (inode->i_size < PMFS_DIR_REC_LEN(1) + PMFS_DIR_REC_LEN(2)) {
		crfs_dbg("bad directory (dir #%lu)-no data block",
			  inode->i_ino);
		return 1;
	}

	blk_base = crfs_get_block(sb, crfs_find_data_block(inode, 0));
	if (!blk_base) {
		crfs_dbg("bad directory (dir #%lu)-no data block",
			  inode->i_ino);
		return 1;
	}

	de = (struct crfs_direntry *)blk_base;
	de1 = crfs_next_entry(de);

	if (le64_to_cpu(de->ino) != inode->i_ino || !le64_to_cpu(de1->ino) ||
	    strcmp(".", de->name) || strcmp("..", de1->name)) {
		crfs_dbg("bad directory (dir #%lu) - no `.' or `..'",
			  inode->i_ino);
		return 1;
	}
	offset = le16_to_cpu(de->de_len) + le16_to_cpu(de1->de_len);
	de = crfs_next_entry(de1);
	while (offset < inode->i_size) {
		if (!blk_base || (void *)de >= (void *)(blk_base +
					sb->s_blocksize)) {
			err = 0;
			blk_base = crfs_get_block(sb, crfs_find_data_block(
				    inode, offset >> sb->s_blocksize_bits));
			if (!blk_base) {
				crfs_dbg("Error: reading dir #%lu offset %lu\n",
					  inode->i_ino, offset);
				offset += sb->s_blocksize;
				continue;
			}
			de = (struct crfs_direntry *)blk_base;
		}
		if (!crfs_check_dir_entry("empty_dir", inode, de, blk_base,
					offset)) {
			de = (struct crfs_direntry *)(blk_base +
				sb->s_blocksize);
			offset = (offset | (sb->s_blocksize - 1)) + 1;
			continue;
		}
		if (le64_to_cpu(de->ino))
			return 0;
		offset += le16_to_cpu(de->de_len);
		de = crfs_next_entry(de);
	}
	return 1;
}

static int crfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct crfs_direntry *de;
	crfs_transaction_t *trans;
	struct super_block *sb = inode->i_sb;
	struct crfs_inode *pi = crfs_get_inode(sb, inode->i_ino), *pidir;
	int err = -ENOTEMPTY;

	if (!inode)
		return -ENOENT;

	if (crfs_inode_by_name(dir, &dentry->d_name, &de) == 0)
		return -ENOENT;

	if (!crfs_empty_dir(inode))
		return err;

	if (inode->i_nlink != 2)
		crfs_dbg("empty directory has nlink!=2 (%d)", inode->i_nlink);

	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 2 +
			MAX_DIRENTRY_LENTRIES);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		return err;
	}
	crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	err = crfs_remove_entry(trans, dentry, inode);
	if (err)
		goto end_rmdir;

	/*inode->i_version++; */
	clear_nlink(inode);
	inode->i_ctime = dir->i_ctime;

	crfs_memunlock_inode(sb, pi);
	pi->i_links_count = cpu_to_le16(inode->i_nlink);
	pi->i_ctime = cpu_to_le64(inode->i_ctime.tv_sec);
	crfs_memlock_inode(sb, pi);

	/* add the inode to truncate list in case a crash happens before the
	 * subsequent evict_inode is called. It will be deleted from the
	 * truncate list during evict_inode.
	 */
	crfs_truncate_add(inode, inode->i_size);

	pidir = crfs_get_inode(sb, dir->i_ino);
	crfs_dec_count(dir, pidir);

	crfs_commit_transaction(sb, trans);
	return err;
end_rmdir:
	crfs_abort_transaction(sb, trans);
	return err;
}

static int crfs_rename(struct inode *old_dir,
			struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
	struct crfs_direntry *new_de = NULL, *old_de = NULL;
	crfs_transaction_t *trans;
	struct super_block *sb = old_inode->i_sb;
	struct crfs_inode *pi, *new_pidir, *old_pidir;
	int err = -ENOENT;

	crfs_inode_by_name(new_dir, &new_dentry->d_name, &new_de);
	crfs_inode_by_name(old_dir, &old_dentry->d_name, &old_de);

	trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES * 4 +
			MAX_DIRENTRY_LENTRIES * 2);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out;
	}

	if (new_inode) {
		err = -ENOTEMPTY;
		if (S_ISDIR(old_inode->i_mode) && !crfs_empty_dir(new_inode))
			goto out;
	} else {
		if (S_ISDIR(old_inode->i_mode)) {
			err = -EMLINK;
			if (new_dir->i_nlink >= PMFS_LINK_MAX)
				goto out;
		}
	}

	new_pidir = crfs_get_inode(sb, new_dir->i_ino);

	pi = crfs_get_inode(sb, old_inode->i_ino);
	crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	if (!new_de) {
		/* link it into the new directory. */
		err = crfs_add_entry(trans, new_dentry, old_inode);
		if (err)
			goto out;
	} else {
		crfs_add_logentry(sb, trans, &new_de->ino, sizeof(new_de->ino),
			LE_DATA);

		crfs_memunlock_range(sb, new_de, sb->s_blocksize);
		new_de->ino = cpu_to_le64(old_inode->i_ino);
		/*new_de->file_type = old_de->file_type; */
		crfs_memlock_range(sb, new_de, sb->s_blocksize);

		crfs_add_logentry(sb, trans, new_pidir, MAX_DATA_PER_LENTRY,
			LE_DATA);
		/*new_dir->i_version++; */
		new_dir->i_ctime = new_dir->i_mtime = CURRENT_TIME_SEC;
		crfs_update_time(new_dir, new_pidir);
	}

	/* and unlink the inode from the old directory ... */
	err = crfs_remove_entry(trans, old_dentry, old_inode);
	if (err)
		goto out;

	if (new_inode) {
		pi = crfs_get_inode(sb, new_inode->i_ino);
		crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);
		new_inode->i_ctime = CURRENT_TIME;

		crfs_memunlock_inode(sb, pi);
		if (S_ISDIR(old_inode->i_mode)) {
			if (new_inode->i_nlink)
				drop_nlink(new_inode);
		}
		pi->i_ctime = cpu_to_le32(new_inode->i_ctime.tv_sec);
		if (new_inode->i_nlink)
			drop_nlink(new_inode);
		pi->i_links_count = cpu_to_le16(new_inode->i_nlink);
		crfs_memlock_inode(sb, pi);

		if (!new_inode->i_nlink)
			crfs_truncate_add(new_inode, new_inode->i_size);
	} else {
		if (S_ISDIR(old_inode->i_mode)) {
			crfs_inc_count(new_dir, new_pidir);
			old_pidir = crfs_get_inode(sb, old_dir->i_ino);
			crfs_dec_count(old_dir, old_pidir);
		}
	}

	crfs_commit_transaction(sb, trans);
	return 0;
out:
	crfs_abort_transaction(sb, trans);
	return err;
}

struct dentry *crfs_get_parent(struct dentry *child)
{
	struct inode *inode;
	struct qstr dotdot = QSTR_INIT("..", 2);
	struct crfs_direntry *de = NULL;
	ino_t ino;

	crfs_inode_by_name(child->d_inode, &dotdot, &de);
	if (!de)
		return ERR_PTR(-ENOENT);
	ino = le64_to_cpu(de->ino);

	if (ino)
		inode = crfs_iget(child->d_inode->i_sb, ino);
	else
		return ERR_PTR(-ENOENT);

	return d_obtain_alias(inode);
}

const struct inode_operations crfs_dir_inode_operations = {
	.create		= crfs_create,
	.lookup		= crfs_lookup,
	.link		= crfs_link,
	.unlink		= crfs_unlink,
	.symlink	= crfs_symlink,
	.mkdir		= crfs_mkdir,
	.rmdir		= crfs_rmdir,
	.mknod		= crfs_mknod,
	.rename		= crfs_rename,
	.setattr	= crfs_notify_change,
	.get_acl	= NULL,
};

const struct inode_operations crfs_special_inode_operations = {
	.setattr	= crfs_notify_change,
	.get_acl	= NULL,
};
