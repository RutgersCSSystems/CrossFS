/*
 * BRIEF DESCRIPTION
 *
 * Memory protection definitions for the PMFS filesystem.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2010-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __WPROTECT_H
#define __WPROTECT_H

#include <linux/pmfs_def.h>
#include <linux/fs.h>

/* crfs_memunlock_super() before calling! */
static inline void crfs_sync_super(struct crfs_super_block *ps)
{
	u16 crc = 0;

	ps->s_wtime = cpu_to_le32(get_seconds());
	ps->s_sum = 0;
	crc = crc16(~0, (__u8 *)ps + sizeof(__le16),
			PMFS_SB_STATIC_SIZE(ps) - sizeof(__le16));
	ps->s_sum = cpu_to_le16(crc);
	/* Keep sync redundant super block */
	memcpy((void *)ps + PMFS_SB_SIZE, (void *)ps,
		sizeof(struct crfs_super_block));
}

#if 1
/* crfs_memunlock_inode() before calling! */
static inline void crfs_sync_inode(struct crfs_inode *pi)
{
	u16 crc = 0;

	//pi->i_sum = 0;
	crc = crc16(~0, (__u8 *)pi + sizeof(__le16), PMFS_INODE_SIZE -
		    sizeof(__le16));
	//pi->i_sum = cpu_to_le16(crc);
}
#endif

extern int crfs_writeable(void *vaddr, unsigned long size, int rw);
extern int crfs_xip_mem_protect(struct super_block *sb,
				 void *vaddr, unsigned long size, int rw);

extern spinlock_t crfs_writeable_lock;
static inline int crfs_is_protected(struct super_block *sb)
{
	struct crfs_sb_info *sbi = (struct crfs_sb_info *)sb->s_fs_info;

	return sbi->s_mount_opt & PMFS_MOUNT_PROTECT;
}

static inline int crfs_is_protected_old(struct super_block *sb)
{
	struct crfs_sb_info *sbi = (struct crfs_sb_info *)sb->s_fs_info;

	return sbi->s_mount_opt & PMFS_MOUNT_PROTECT_OLD;
}

static inline int crfs_is_wprotected(struct super_block *sb)
{
	return crfs_is_protected(sb) || crfs_is_protected_old(sb);
}

static inline void
__crfs_memunlock_range(void *p, unsigned long len, int hold_lock)
{

#if 1
	/*
	 * NOTE: Ideally we should lock all the kernel to be memory safe
	 * and avoid to write in the protected memory,
	 * obviously it's not possible, so we only serialize
	 * the operations at fs level. We can't disable the interrupts
	 * because we could have a deadlock in this path.
	 */
	if (hold_lock)
		spin_lock(&crfs_writeable_lock);
	crfs_writeable(p, len, 1);
#endif
}

static inline void
__crfs_memlock_range(void *p, unsigned long len, int hold_lock)
{
#if 1
	crfs_writeable(p, len, 0);
	if (hold_lock)
		spin_unlock(&crfs_writeable_lock);
#endif
}

static inline void crfs_memunlock_range(struct super_block *sb, void *p,
					 unsigned long len)
{
#if 1
	if (crfs_is_protected(sb))
		__crfs_memunlock_range(p, len, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memunlock_range(p, len, 1);
#endif
}

static inline void crfs_memlock_range(struct super_block *sb, void *p,
				       unsigned long len)
{

#if 1
	if (crfs_is_protected(sb))
		__crfs_memlock_range(p, len, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memlock_range(p, len, 1);
#endif
}

static inline void crfs_memunlock_super(struct super_block *sb,
					 struct crfs_super_block *ps)
{
#if 1
	if (crfs_is_protected(sb))
		__crfs_memunlock_range(ps, PMFS_SB_SIZE, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memunlock_range(ps, PMFS_SB_SIZE, 1);
#endif
}

static inline void crfs_memlock_super(struct super_block *sb,
				       struct crfs_super_block *ps)
{
#if 1
	crfs_sync_super(ps);
	if (crfs_is_protected(sb))
		__crfs_memlock_range(ps, PMFS_SB_SIZE, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memlock_range(ps, PMFS_SB_SIZE, 1);
#endif
}

static inline void crfs_memunlock_inode(struct super_block *sb,
					 struct crfs_inode *pi)
{
#if 1
	if (crfs_is_protected(sb))
		__crfs_memunlock_range(pi, PMFS_SB_SIZE, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memunlock_range(pi, PMFS_SB_SIZE, 1);
#endif
}

static inline void crfs_memlock_inode(struct super_block *sb,
				       struct crfs_inode *pi)
{
#if 1
	/* crfs_sync_inode(pi); */
	if (crfs_is_protected(sb))
		__crfs_memlock_range(pi, PMFS_SB_SIZE, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memlock_range(pi, PMFS_SB_SIZE, 1);
#endif
}

static inline void crfs_memunlock_block(struct super_block *sb, void *bp)
{
#if 1
	if (crfs_is_protected(sb))
		__crfs_memunlock_range(bp, sb->s_blocksize, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memunlock_range(bp, sb->s_blocksize, 1);
#endif
}

static inline void crfs_memlock_block(struct super_block *sb, void *bp)
{
#if 1
	if (crfs_is_protected(sb))
		__crfs_memlock_range(bp, sb->s_blocksize, 0);
	else if (crfs_is_protected_old(sb))
		__crfs_memlock_range(bp, sb->s_blocksize, 1);
#endif
}

#endif
