/*
 * BRIEF DESCRIPTION
 *
 * File operations for directories.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * change log
 *
 * vfs_readdir() is gone; switch to iterate_dir() instead
 * filldir() API is changed!
 *
 */

#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/pagemap.h>
#include "pmfs.h"

/*
 *	Parent is locked.
 */

#define DT2IF(dt) (((dt) << 12) & S_IFMT)
#define IF2DT(sif) (((sif) & S_IFMT) >> 12)

static int crfs_add_dirent_to_buf(crfs_transaction_t *trans,
	struct dentry *dentry, struct inode *inode,
	struct crfs_direntry *de, u8 *blk_base,  struct crfs_inode *pidir)
{
	struct inode *dir = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	unsigned short reclen;
	int nlen, rlen;
	char *top;

	reclen = PMFS_DIR_REC_LEN(namelen);
	if (!de) {
		de = (struct crfs_direntry *)blk_base;
		top = blk_base + dir->i_sb->s_blocksize - reclen;
		while ((char *)de <= top) {
#if 0
			if (!crfs_check_dir_entry("crfs_add_dirent_to_buf",
			    dir, de, blk_base, offset))
				return -EIO;
			if (crfs_match(namelen, name, de))
				return -EEXIST;
#endif
			rlen = le16_to_cpu(de->de_len);
			if (de->ino) {
				nlen = PMFS_DIR_REC_LEN(de->name_len);
				if ((rlen - nlen) >= reclen)
					break;
			} else if (rlen >= reclen)
				break;
			de = (struct crfs_direntry *)((char *)de + rlen);
		}
		if ((char *)de > top)
			return -ENOSPC;
	}
	rlen = le16_to_cpu(de->de_len);

	if (de->ino) {
		struct crfs_direntry *de1;
		crfs_add_logentry(dir->i_sb, trans, &de->de_len,
			sizeof(de->de_len), LE_DATA);
		nlen = PMFS_DIR_REC_LEN(de->name_len);
		de1 = (struct crfs_direntry *)((char *)de + nlen);
		crfs_memunlock_block(dir->i_sb, blk_base);
		de1->de_len = cpu_to_le16(rlen - nlen);
		de->de_len = cpu_to_le16(nlen);
		crfs_memlock_block(dir->i_sb, blk_base);
		de = de1;
	} else {
		crfs_add_logentry(dir->i_sb, trans, &de->ino,
			sizeof(de->ino), LE_DATA);
	}
	crfs_memunlock_block(dir->i_sb, blk_base);
	/*de->file_type = 0;*/
	if (inode) {
		de->ino = cpu_to_le64(inode->i_ino);
		/*de->file_type = IF2DT(inode->i_mode); */
	} else {
		de->ino = 0;
	}
	de->name_len = namelen;
	memcpy(de->name, name, namelen);
	crfs_memlock_block(dir->i_sb, blk_base);
	crfs_flush_buffer(de, reclen, false);
	/*
	 * XXX shouldn't update any times until successful
	 * completion of syscall, but too many callers depend
	 * on this.
	 */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
	/*dir->i_version++; */

	crfs_memunlock_inode(dir->i_sb, pidir);
	pidir->i_mtime = cpu_to_le32(dir->i_mtime.tv_sec);
	pidir->i_ctime = cpu_to_le32(dir->i_ctime.tv_sec);
	crfs_memlock_inode(dir->i_sb, pidir);
	return 0;
}

/* adds a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int crfs_add_entry(crfs_transaction_t *trans, struct dentry *dentry,
		struct inode *inode)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block *sb = dir->i_sb;
	int retval = -EINVAL;
	unsigned long block, blocks;
	struct crfs_direntry *de;
	char *blk_base;
	struct crfs_inode *pidir;

	if (!dentry->d_name.len)
		return -EINVAL;

	pidir = crfs_get_inode(sb, dir->i_ino);
	crfs_add_logentry(sb, trans, pidir, MAX_DATA_PER_LENTRY, LE_DATA);

	blocks = dir->i_size >> sb->s_blocksize_bits;
	for (block = 0; block < blocks; block++) {
		blk_base =
			crfs_get_block(sb, crfs_find_data_block(dir, block));
		if (!blk_base) {
			retval = -EIO;
			goto out;
		}
		retval = crfs_add_dirent_to_buf(trans, dentry, inode,
				NULL, blk_base, pidir);
		if (retval != -ENOSPC)
			goto out;
	}
	retval = crfs_alloc_blocks(trans, dir, blocks, 1, false);
	if (retval)
		goto out;

	dir->i_size += dir->i_sb->s_blocksize;
	crfs_update_isize(dir, pidir);

	blk_base = crfs_get_block(sb, crfs_find_data_block(dir, blocks));
	if (!blk_base) {
		retval = -ENOSPC;
		goto out;
	}
	/* No need to log the changes to this de because its a new block */
	de = (struct crfs_direntry *)blk_base;
	crfs_memunlock_block(sb, blk_base);
	de->ino = 0;
	de->de_len = cpu_to_le16(sb->s_blocksize);
	crfs_memlock_block(sb, blk_base);
	/* Since this is a new block, no need to log changes to this block */
	retval = crfs_add_dirent_to_buf(NULL, dentry, inode, de, blk_base,
		pidir);
out:
	return retval;
}

/* removes a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int crfs_remove_entry(crfs_transaction_t *trans, struct dentry *de,
		struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct inode *dir = de->d_parent->d_inode;
	struct crfs_inode *pidir;
	struct qstr *entry = &de->d_name;
	struct crfs_direntry *res_entry, *prev_entry;
	int retval = -EINVAL;
	unsigned long blocks, block;
	char *blk_base = NULL;

	if (!de->d_name.len)
		return -EINVAL;

	blocks = dir->i_size >> sb->s_blocksize_bits;

	for (block = 0; block < blocks; block++) {
		blk_base =
			crfs_get_block(sb, crfs_find_data_block(dir, block));
		if (!blk_base)
			goto out;
		if (crfs_search_dirblock(blk_base, dir, entry,
					  block << sb->s_blocksize_bits,
					  &res_entry, &prev_entry) == 1)
			break;
	}

	if (block == blocks)
		goto out;
	if (prev_entry) {
		crfs_add_logentry(sb, trans, &prev_entry->de_len,
				sizeof(prev_entry->de_len), LE_DATA);
		crfs_memunlock_block(sb, blk_base);
		prev_entry->de_len =
			cpu_to_le16(le16_to_cpu(prev_entry->de_len) +
				    le16_to_cpu(res_entry->de_len));
		crfs_memlock_block(sb, blk_base);
	} else {
		crfs_add_logentry(sb, trans, &res_entry->ino,
				sizeof(res_entry->ino), LE_DATA);
		crfs_memunlock_block(sb, blk_base);
		res_entry->ino = 0;
		crfs_memlock_block(sb, blk_base);
	}
	/*dir->i_version++; */
	dir->i_ctime = dir->i_mtime = CURRENT_TIME_SEC;

	pidir = crfs_get_inode(sb, dir->i_ino);
	crfs_add_logentry(sb, trans, pidir, MAX_DATA_PER_LENTRY, LE_DATA);

	crfs_memunlock_inode(sb, pidir);
	pidir->i_mtime = cpu_to_le32(dir->i_mtime.tv_sec);
	pidir->i_ctime = cpu_to_le32(dir->i_ctime.tv_sec);
	crfs_memlock_inode(sb, pidir);
	retval = 0;
out:
	return retval;
}


static int crfs_readdir(struct file *filp, struct dir_context *ctx)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct crfs_inode *pi;
	char *blk_base;
	int ret = 0, stored;
	int error = 0;
	unsigned long offset;
	struct crfs_direntry *de;
	ino_t ino;

	stored = 0;
	offset = ctx->pos & (sb->s_blocksize - 1);
	while (!error && !stored && ctx->pos < inode->i_size) {
		unsigned long blk = ctx->pos >> sb->s_blocksize_bits;

		blk_base =
			crfs_get_block(sb, crfs_find_data_block(inode, blk));
		if (!blk_base) {
			crfs_dbg("directory %lu contains a hole at offset %lld\n",
				inode->i_ino, ctx->pos);
			ctx->pos += sb->s_blocksize - offset;
			continue;
		}
#if 0
		if (filp->f_version != inode->i_version) {
			for (i = 0; i < sb->s_blocksize && i < offset; ) {
				de = (struct crfs_direntry *)(blk_base + i);
				/* It's too expensive to do a full
				 * dirent test each time round this
				 * loop, but we do have to test at
				 * least that it is non-zero.  A
				 * failure will be detected in the
				 * dirent test below. */
				if (le16_to_cpu(de->de_len) <
				    PMFS_DIR_REC_LEN(1))
					break;
				i += le16_to_cpu(de->de_len);
			}
			offset = i;
			ctx->pos =
				(ctx->pos & ~(sb->s_blocksize - 1)) | offset;
			filp->f_version = inode->i_version;
		}
#endif
		while (!error && ctx->pos < inode->i_size
		       && offset < sb->s_blocksize) {
			de = (struct crfs_direntry *)(blk_base + offset);
			if (!crfs_check_dir_entry("crfs_readdir", inode, de,
						   blk_base, offset)) {
				/* On error, skip the f_pos to the next block. */
				ctx->pos = (ctx->pos | (sb->s_blocksize - 1)) + 1;
				ret = stored;
				goto out;
			}
			offset += le16_to_cpu(de->de_len);
			if (de->ino) {
				ino = le64_to_cpu(de->ino);
				pi = crfs_get_inode(sb, ino);
				/*error = filldir(ctx, de->name, de->name_len,
						ctx->pos, ino,
						IF2DT(le16_to_cpu(pi->i_mode)));*/

				// dir_emit() return 1 means sucess, 0 means fail

				error = (dir_emit(ctx, de->name, de->name_len, ino, IF2DT(le16_to_cpu(pi->i_mode))) == 0);

				if (error) {
					break;
				}
				stored++;
			}
			ctx->pos += le16_to_cpu(de->de_len);
		}
		offset = 0;
	}
out:
	return ret;
}

#if 0
static int crfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct crfs_inode *pi;
	char *blk_base;
	int ret = 0, stored;
	int error = 0;
	unsigned long offset;
	struct crfs_direntry *de;
	ino_t ino;

	stored = 0;
	offset = filp->f_pos & (sb->s_blocksize - 1);
	while (!error && !stored && filp->f_pos < inode->i_size) {
		unsigned long blk = filp->f_pos >> sb->s_blocksize_bits;

		blk_base =
			crfs_get_block(sb, crfs_find_data_block(inode, blk));
		if (!blk_base) {
			crfs_dbg("directory %lu contains a hole at offset %lld\n",
				inode->i_ino, filp->f_pos);
			filp->f_pos += sb->s_blocksize - offset;
			continue;
		}
#if 0
		if (filp->f_version != inode->i_version) {
			for (i = 0; i < sb->s_blocksize && i < offset; ) {
				de = (struct crfs_direntry *)(blk_base + i);
				/* It's too expensive to do a full
				 * dirent test each time round this
				 * loop, but we do have to test at
				 * least that it is non-zero.  A
				 * failure will be detected in the
				 * dirent test below. */
				if (le16_to_cpu(de->de_len) <
				    PMFS_DIR_REC_LEN(1))
					break;
				i += le16_to_cpu(de->de_len);
			}
			offset = i;
			filp->f_pos =
				(filp->f_pos & ~(sb->s_blocksize - 1)) | offset;
			filp->f_version = inode->i_version;
		}
#endif
		while (!error && filp->f_pos < inode->i_size
		       && offset < sb->s_blocksize) {
			de = (struct crfs_direntry *)(blk_base + offset);
			if (!crfs_check_dir_entry("crfs_readdir", inode, de,
						   blk_base, offset)) {
				/* On error, skip the f_pos to the next block. */
				filp->f_pos = (filp->f_pos | (sb->s_blocksize - 1)) + 1;
				ret = stored;
				goto out;
			}
			offset += le16_to_cpu(de->de_len);
			if (de->ino) {
				ino = le64_to_cpu(de->ino);
				pi = crfs_get_inode(sb, ino);
				error = filldir(dirent, de->name, de->name_len,
						filp->f_pos, ino,
						IF2DT(le16_to_cpu(pi->i_mode)));
				if (error)
					break;
				stored++;
			}
			filp->f_pos += le16_to_cpu(de->de_len);
		}
		offset = 0;
	}
out:
	return ret;
}
#endif

const struct file_operations crfs_dir_operations = {
	.read		= generic_read_dir,
	//.readdir	= crfs_readdir,
	.iterate	= crfs_readdir,
	.iterate_shared = crfs_readdir,
	.fsync		= noop_fsync,
	//.unlocked_ioctl = crfs_ioctl,
        .unlocked_ioctl = crfss_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= crfs_compat_ioctl,
#endif
};
