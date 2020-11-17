/*
 * FILE NAME include/linux/nova_DEVFS_fs.h
 *
 * BRIEF DESCRIPTION
 *
 * Definitions for the NOVA filesystem.
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef _LINUX_DEVFS_DEF_H
#define _LINUX_DEVFS_DEF_H

#include <linux/types.h>
#include <linux/magic.h>

#define	DEVFS_SUPER_MAGIC	0x4E4F5641	/* NOVA */

/*
 * The NOVA filesystem constants/structures
 */

/*
 * Mount flags
 */
#define DEVFS_MOUNT_PROTECT 0x000001            /* wprotect CR0.WP */
#define DEVFS_MOUNT_XATTR_USER 0x000002         /* Extended user attributes */
#define DEVFS_MOUNT_POSIX_ACL 0x000004          /* POSIX Access Control Lists */
#define DEVFS_MOUNT_DAX 0x000008                /* Direct Access */
#define DEVFS_MOUNT_ERRORS_CONT 0x000010        /* Continue on errors */
#define DEVFS_MOUNT_ERRORS_RO 0x000020          /* Remount fs ro on errors */
#define DEVFS_MOUNT_ERRORS_PANIC 0x000040       /* Panic on errors */
#define DEVFS_MOUNT_HUGEMMAP 0x000080           /* Huge mappings with mmap */
#define DEVFS_MOUNT_HUGEIOREMAP 0x000100        /* Huge mappings with ioremap */
#define DEVFS_MOUNT_FORMAT      0x000200        /* was FS formatted on mount? */
#define DEVFS_MOUNT_MOUNTING    0x000400        /* FS currently being mounted */

/*
 * Maximal count of links to a file
 */
#define DEVFS_LINK_MAX          32000

#define DEVFS_DEF_BLOCK_SIZE_4K 4096

#define DEVFS_INODE_SIZE 128    /* must be power of two */
#define DEVFS_INODE_BITS   7

#define DEVFS_NAME_LEN 255

/* NOVA supported data blocks */
#define DEVFS_BLOCK_TYPE_4K     0
#define DEVFS_BLOCK_TYPE_2M     1
#define DEVFS_BLOCK_TYPE_1G     2
#define DEVFS_BLOCK_TYPE_MAX    3

#define META_BLK_SHIFT 9

/*
 * Play with this knob to change the default block type.
 * By changing the DEVFS_DEFAULT_BLOCK_TYPE to 2M or 1G,
 * we should get pretty good coverage in testing.
 */
#define DEVFS_DEFAULT_BLOCK_TYPE DEVFS_BLOCK_TYPE_4K


#if 0

/*
 * Structure of an inode in NOVA.
 * Keep the inode size to within 120 bytes: We use the last eight bytes
 * as inode table tail pointer.
 */
struct devfss_inode {
	/* first 48 bytes */
	__le16	i_rsvd;		/* reserved. used to be checksum */
	u8	valid;		/* Is this inode valid? */
	u8	i_blk_type;	/* data block size this inode uses */
	__le32	i_flags;	/* Inode flags */
	__le64	i_size;		/* Size of data in bytes */
	__le32	i_ctime;	/* Inode modification time */
	__le32	i_mtime;	/* Inode b-tree Modification time */
	__le32	i_atime;	/* Access time */
	__le16	i_mode;		/* File mode */
	__le16	i_links_count;	/* Links count */

	/*
	 * Blocks count. This field is updated in-place;
	 * We just make sure it is consistent upon clean umount,
	 * and it is recovered in DFS recovery if power failure occurs.
	 */
	__le64	i_blocks;
	__le64	i_xattr;	/* Extended attribute block */

	/* second 48 bytes */
	__le32	i_uid;		/* Owner Uid */
	__le32	i_gid;		/* Group Id */
	__le32	i_generation;	/* File version (for NFS) */
	__le32	padding;
	__le64	nova_ino;	/* nova inode number */

	__le64	log_head;	/* Log head pointer */
	__le64	log_tail;	/* Log tail pointer */

	struct {
		__le32 rdev;	/* major/minor # */
	} dev;			/* device inode */

	//PMFS flag
	u8          height;         /* height of data b-tree; max 3 for now */

	/* Leave 8 bytes for inode table tail pointer */
} __attribute((__packed__));
#endif

/*
 * Structure of an inode in PMFS. Things to keep in mind when modifying it.
 * 1) Keep the inode size to within 96 bytes if possible. This is because
 *    a 64 byte log-entry can store 48 bytes of data and we would like
 *    to log an inode using only 2 log-entries
 * 2) root must be immediately after the qw containing height because we update
 *    root and height atomically using cmpxchg16b in pmfs_decrease_btree_height 
 * 3) i_size, i_ctime, and i_mtime must be in that order and i_size must be at
 *    16 byte aligned offset from the start of the inode. We use cmpxchg16b to
 *    update these three fields atomically.
 */
struct devfss_inode {
        /* first 48 bytes */
        __le16  i_rsvd;         /* reserved. used to be checksum */
        u8          height;         /* height of data b-tree; max 3 for now */
        u8          i_blk_type;     /* data block size this inode uses */
        __le32  i_flags;            /* Inode flags */
        __le64  root;               /* btree root. must be below qw w/ height */
        __le64  i_size;             /* Size of data in bytes */
        __le32  i_ctime;            /* Inode modification time */
        __le32  i_mtime;            /* Inode b-tree Modification time */
        __le32  i_dtime;            /* Deletion Time */
        __le16  i_mode;             /* File mode */
        __le16  i_links_count;      /* Links count */
        __le64  i_blocks;           /* Blocks count */

        /* second 48 bytes */
        __le64  i_xattr;            /* Extended attribute block */
        __le32  i_uid;              /* Owner Uid */
        __le32  i_gid;              /* Group Id */
        __le32  i_generation;       /* File version (for NFS) */
        __le32  i_atime;            /* Access time */

        struct {
                __le32 rdev;    /* major/minor # */
        } dev;              /* device inode */
        __le32 padding;     /* pad to ensure truncate_item starts 8-byte aligned */
};



#define DEVFS_SB_SIZE 512       /* must be power of two */


/*
 * Structure of the super block in NOVA
 * The fields are partitioned into static and dynamic fields. The static fields
 * never change after file system creation. This was primarily done because
 * nova_get_block() returns NULL if the block offset is 0 (helps in catching
 * bugs). So if we modify any field using journaling (for consistency), we
 * will have to modify s_sum which is at offset 0. So journaling code fails.
 * This (static+dynamic fields) is a temporary solution and can be avoided
 * once the file system becomes stable and nova_get_block() returns correct
 * pointers even for offset 0.
 */
struct devfs_super_block {
	/* static fields. they never change after file system creation.
	 * checksum only validates up to s_start_dynamic field below */
	__le16		s_sum;              /* checksum of this sb */
	__le16		s_padding16;
	__le32		s_magic;            /* magic signature */
	__le32		s_padding32;
	__le32		s_blocksize;        /* blocksize in bytes */
	__le64		s_size;             /* total size of fs in bytes */
	char		s_volume_name[16];  /* volume name */

	__le64		s_start_dynamic;

	/* all the dynamic fields should go here */
	/* s_mtime and s_wtime should be together and their order should not be
	 * changed. we use an 8 byte write to update both of them atomically */
	__le32		s_mtime;            /* mount time */
	__le32		s_wtime;            /* write time */
	/* fields for fast mount support. Always keep them together */
	__le64		s_num_free_blocks;
} __attribute((__packed__));

#define DEVFS_SB_STATIC_SIZE(ps) ((u64)&ps->s_start_dynamic - (u64)ps)

/* the above fast mount fields take total 32 bytes in the super block */
#define DEVFS_FAST_MOUNT_FIELD_SIZE  (36)

/* The root inode follows immediately after the redundant super block */
#define DEVFS_ROOT_INO		(1)
#define DEVFS_INODETABLE_INO	(2)	/* Temporaty inode table */
#define DEVFS_BLOCKNODE_INO	(3)
#define DEVFS_INODELIST_INO	(4)
#define DEVFS_LITEJOURNAL_INO	(5)
#define DEVFS_INODELIST1_INO	(6)

#define	DEVFS_ROOT_INO_START	(DEVFS_SB_SIZE * 2)

/* Normal inode starts at 16 */
#define DEVFS_NORMAL_INODE_START      (16)

/* ======================= Write ordering ========================= */

#define CACHELINE_SIZE  (64)
#define CACHELINE_MASK  (~(CACHELINE_SIZE - 1))
#define CACHELINE_ALIGN(addr) (((addr)+CACHELINE_SIZE-1) & CACHELINE_MASK)

#define X86_FEATURE_PCOMMIT	( 9*32+22) /* PCOMMIT instruction */
#define X86_FEATURE_CLFLUSHOPT	( 9*32+23) /* CLFLUSHOPT instruction */
#define X86_FEATURE_CLWB	( 9*32+24) /* CLWB instruction */

static inline bool arch_has_pcommit(void)
{
	return static_cpu_has(X86_FEATURE_PCOMMIT);
}

static inline bool arch_has_clwb(void)
{
	return static_cpu_has(X86_FEATURE_CLWB);
}

extern int support_clwb;
extern int support_pcommit;

#define _mm_clflush(addr)\
	asm volatile("clflush %0" : "+m" (*(volatile char *)(addr)))
#define _mm_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(addr)))
#define _mm_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(addr)))
#define _mm_pcommit()\
	asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8")

/* Provides ordering from all previous clflush too */
static inline void DEVFS_PERSISTENT_MARK(void)
{
	/* TODO: Fix me. */
}

static inline void DEVFS_PERSISTENT_BARRIER(void)
{
	asm volatile ("sfence\n" : : );
	if (support_pcommit) {
		/* Do nothing */
	}
}

static inline void devfs_flush_buffer(void *buf, uint32_t len, bool fence)
{
	uint32_t i;
	len = len + ((unsigned long)(buf) & (CACHELINE_SIZE - 1));
	if (support_clwb) {
		for (i = 0; i < len; i += CACHELINE_SIZE)
			_mm_clwb(buf + i);
	} else {
		for (i = 0; i < len; i += CACHELINE_SIZE)
			_mm_clflush(buf + i);
	}
	/* Do a fence only if asked. We often don't need to do a fence
	 * immediately after clflush because even if we get context switched
	 * between clflush and subsequent fence, the context switch operation
	 * provides implicit fence. */
	if (fence)
		DEVFS_PERSISTENT_BARRIER();
}

#endif /* _LINUX_DEVFS_DEF_H */
