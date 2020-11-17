/*
 * BRIEF DESCRIPTION
 *
 * Definitions for the PMFS filesystem.
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
#ifndef __PMFS_H
#define __PMFS_H

#include <linux/buffer_head.h>
#include <linux/pmfs_def.h>
#include <linux/pmfs_sb.h>
#include <linux/crc16.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/devfs.h>
#include "wprotect.h"
#include "journal.h"

#define PAGE_SHIFT_2M 21
#define PAGE_SHIFT_1G 30

#define PMFS_ASSERT(x)                                                 \
	if (!(x)) {                                                     \
		printk(KERN_WARNING "assertion failed %s:%d: %s\n",     \
	               __FILE__, __LINE__, #x);                         \
	}

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

/* #define crfs_dbg(s, args...)         pr_debug(s, ## args) */
//#define crfs_dbg(s, args ...)           pr_info(s, ## args)
#define crfs_dbg(s, args ...)           
#define crfs_dbg1(s, args ...)
#define crfs_err(sb, s, args ...)       crfs_error_mng(sb, s, ## args)
#define crfs_warn(s, args ...)          pr_warning(s, ## args)
#define crfs_info(s, args ...)          pr_info(s, ## args)

extern unsigned int crfs_dbgmask;
#define PMFS_DBGMASK_MMAPHUGE          (0x00000001)
#define PMFS_DBGMASK_MMAP4K            (0x00000002)
#define PMFS_DBGMASK_MMAPVERBOSE       (0x00000004)
#define PMFS_DBGMASK_MMAPVVERBOSE      (0x00000008)
#define PMFS_DBGMASK_VERBOSE           (0x00000010)
#define PMFS_DBGMASK_TRANSACTION       (0x00000020)

#define crfs_dbg_mmaphuge(s, args ...)		 \
	(void) ((crfs_dbgmask & PMFS_DBGMASK_MMAPHUGE) ? crfs_dbg(s, args) : 0)
#define crfs_dbg_mmap4k(s, args ...)		 \
	(void) ((crfs_dbgmask & PMFS_DBGMASK_MMAP4K) ? crfs_dbg(s, args) : 0)
#define crfs_dbg_mmapv(s, args ...)		 \
	(void) ((crfs_dbgmask & PMFS_DBGMASK_MMAPVERBOSE) ? crfs_dbg(s, args) : 0)
#define crfs_dbg_mmapvv(s, args ...)		 \
	(void) ((crfs_dbgmask & PMFS_DBGMASK_MMAPVVERBOSE) ? crfs_dbg(s, args) : 0)

#define crfs_dbg_verbose(s, args ...)		 \
	(void) ((crfs_dbgmask & PMFS_DBGMASK_VERBOSE) ? crfs_dbg(s "%s:%d", ##args,__FILE__, __LINE__) : 0)
#define crfs_dbg_trans(s, args ...)		 \
	(void) ((crfs_dbgmask & PMFS_DBGMASK_TRANSACTION) ? crfs_dbg(s, ##args) : 0)

#define crfs_set_bit                   __test_and_set_bit_le
#define crfs_clear_bit                 __test_and_clear_bit_le
#define crfs_find_next_zero_bit                find_next_zero_bit_le

#define clear_opt(o, opt)       (o &= ~PMFS_MOUNT_ ## opt)
#define set_opt(o, opt)         (o |= PMFS_MOUNT_ ## opt)
#define test_opt(sb, opt)       (PMFS_SB(sb)->s_mount_opt & PMFS_MOUNT_ ## opt)

#define PMFS_LARGE_INODE_TABLE_SIZE    (0x200000)
/* PMFS size threshold for using 2M blocks for inode table */
#define PMFS_LARGE_INODE_TABLE_THREASHOLD    (0x20000000)
/*
 * pmfs inode flags
 *
 * PMFS_EOFBLOCKS_FL	There are blocks allocated beyond eof
 */
#define PMFS_EOFBLOCKS_FL      0x20000000
/* Flags that should be inherited by new inodes from their parent. */
#define PMFS_FL_INHERITED (FS_SECRM_FL | FS_UNRM_FL | FS_COMPR_FL | \
			    FS_SYNC_FL | FS_NODUMP_FL | FS_NOATIME_FL |	\
			    FS_COMPRBLK_FL | FS_NOCOMP_FL | FS_JOURNAL_DATA_FL | \
			    FS_NOTAIL_FL | FS_DIRSYNC_FL)
/* Flags that are appropriate for regular files (all but dir-specific ones). */
#define PMFS_REG_FLMASK (~(FS_DIRSYNC_FL | FS_TOPDIR_FL))
/* Flags that are appropriate for non-directories/regular files. */
#define PMFS_OTHER_FLMASK (FS_NODUMP_FL | FS_NOATIME_FL)
#define PMFS_FL_USER_VISIBLE (FS_FL_USER_VISIBLE | PMFS_EOFBLOCKS_FL)

#define INODES_PER_BLOCK(bt) (1 << (blk_type_to_shift[bt] - PMFS_INODE_BITS))

#define _ENABLE_PMFSJOURN

//#define MAX_FP_QSIZE (128)

extern unsigned int blk_type_to_shift[PMFS_BLOCK_TYPE_MAX];
extern unsigned int blk_type_to_size[PMFS_BLOCK_TYPE_MAX];

/* Function Prototypes */
extern void crfs_error_mng(struct super_block *sb, const char *fmt, ...);

/* file.c */
extern int crfs_mmap(struct file *file, struct vm_area_struct *vma);

/* balloc.c */
int crfs_setup_blocknode_map(struct super_block *sb);
extern struct crfs_blocknode *crfs_alloc_blocknode(struct super_block *sb);
extern void crfs_free_blocknode(struct super_block *sb, struct crfs_blocknode *bnode);
extern void crfs_init_blockmap(struct super_block *sb,
		unsigned long init_used_size);
extern void crfs_free_block(struct super_block *sb, unsigned long blocknr,
	unsigned short btype);
extern void __crfs_free_block(struct super_block *sb, unsigned long blocknr,
	unsigned short btype, struct crfs_blocknode **start_hint);
extern int crfs_new_block(struct super_block *sb, unsigned long *blocknr,
	unsigned short btype, int zero);
extern unsigned long crfs_count_free_blocks(struct super_block *sb);

/* dir.c */
extern int crfs_add_entry(crfs_transaction_t *trans,
		struct dentry *dentry, struct inode *inode);
extern int crfs_remove_entry(crfs_transaction_t *trans,
		struct dentry *dentry, struct inode *inode);

/* namei.c */
extern struct dentry *crfs_get_parent(struct dentry *child);
extern int crfs_unlink(struct inode *dir, struct dentry *dentry);

/* inode.c */
extern unsigned int crfs_free_inode_subtree(struct super_block *sb,
		u64 root, u32 height, u32 btype, unsigned long last_blocknr);
extern int __crfs_alloc_blocks(crfs_transaction_t *trans,
		struct super_block *sb, struct crfs_inode *pi,
		unsigned long file_blocknr, unsigned int num, bool zero);
extern int crfs_init_inode_table(struct super_block *sb);
extern int crfs_alloc_blocks(crfs_transaction_t *trans, struct inode *inode,
		unsigned long file_blocknr, unsigned int num, bool zero);
extern u64 crfs_find_data_block(struct inode *inode,
	unsigned long file_blocknr);
int crfs_set_blocksize_hint(struct super_block *sb, struct crfs_inode *pi,
		loff_t new_size);
void crfs_setsize(struct inode *inode, loff_t newsize);

void crfs_update_inode(struct inode *inode, struct crfs_inode *pi);


extern struct inode *crfs_iget(struct super_block *sb, unsigned long ino);
extern void crfs_put_inode(struct inode *inode);
extern void crfs_evict_inode(struct inode *inode);
extern struct inode *crfs_new_inode(crfs_transaction_t *trans,
	struct inode *dir, umode_t mode, const struct qstr *qstr);
extern inline void crfs_update_isize(struct inode *inode,
		struct crfs_inode *pi);
extern inline void crfs_update_nlink(struct inode *inode,
		struct crfs_inode *pi);
extern inline void crfs_update_time(struct inode *inode,
		struct crfs_inode *pi);
extern int crfs_write_inode(struct inode *inode,
	struct writeback_control *wbc);
extern void crfs_dirty_inode(struct inode *inode, int flags);
extern int crfs_notify_change(struct dentry *dentry, struct iattr *attr);
int crfs_getattr(struct vfsmount *mnt, struct dentry *dentry, 
		struct kstat *stat);
extern void crfs_set_inode_flags(struct inode *inode, struct crfs_inode *pi);
extern void crfs_get_inode_flags(struct inode *inode, struct crfs_inode *pi);
extern unsigned long crfs_find_region(struct inode *inode, loff_t *offset,
		int hole);
extern void crfs_truncate_del(struct inode *inode);
extern void crfs_truncate_add(struct inode *inode, u64 truncate_size);

/* ioctl.c */
extern long crfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
extern long crfs_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);
#endif

/* super.c */
#ifdef CONFIG_PMFS_TEST
extern struct crfs_super_block *get_crfs_super(void);
#endif
extern void __crfs_free_blocknode(struct crfs_blocknode *bnode);
extern struct super_block *crfs_read_super(struct super_block *sb, void *data,
	int silent);
extern int crfs_statfs(struct dentry *d, struct kstatfs *buf);
extern int crfs_remount(struct super_block *sb, int *flags, char *data);

int crfs_fill_super(struct super_block *sb, void *data, int silent);


/* Provides ordering from all previous clflush too */
static inline void PERSISTENT_MARK(void)
{
	/* TODO: Fix me. */
}

static inline void PERSISTENT_BARRIER(void)
{
	asm volatile ("sfence\n" : : );
}

static inline void crfs_flush_buffer(void *buf, uint32_t len, bool fence)
{
	uint32_t i;
	len = len + ((unsigned long)(buf) & (CACHELINE_SIZE - 1));
	for (i = 0; i < len; i += CACHELINE_SIZE)
		asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
	/* Do a fence only if asked. We often don't need to do a fence
	 * immediately after clflush because even if we get context switched
	 * between clflush and subsequent fence, the context switch operation
	 * provides implicit fence. */
	if (fence)
		asm volatile ("sfence\n" : : );
}

/* symlink.c */
extern int crfs_block_symlink(struct inode *inode, const char *symname,
	int len);

/* Inline functions start here */

/* Mask out flags that are inappropriate for the given type of inode. */
static inline __le32 crfs_mask_flags(umode_t mode, __le32 flags)
{
	flags &= cpu_to_le32(PMFS_FL_INHERITED);
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & cpu_to_le32(PMFS_REG_FLMASK);
	else
		return flags & cpu_to_le32(PMFS_OTHER_FLMASK);
}

static inline int crfs_calc_checksum(u8 *data, int n)
{
	u16 crc = 0;

	crc = crc16(~0, (__u8 *)data + sizeof(__le16), n - sizeof(__le16));
	if (*((__le16 *)data) == cpu_to_le16(crc))
		return 0;
	else
		return 1;
}

struct crfs_blocknode_lowhigh {
       unsigned long block_low;
       unsigned long block_high;
};
               
struct crfs_blocknode {
	struct list_head link;
	unsigned long block_low;
	unsigned long block_high;
};

struct crfs_inode_vfs {
	__u32   i_dir_start_lookup;
	struct list_head i_truncated;
	struct inode	vfs_inode;

	// The rest of fields are just
	// matching what it is with
	// crfss_inode

        /*Pointer to data blocks */
        __le32  i_data[15];
        /* ACL of the file */
        __u32   i_file_acl;
        /* unlinked but open inodes */
        struct list_head i_orphan;      

        __u32   i_dtime;
        __u32   i_ctime;
        __u32   i_mtime;
        __u64   i_size;
        __u32   i_nlink;

        rwlock_t i_meta_lock;

        /* devfs inode info red-black tree*/
        struct rb_node rbnode;

        /* per crfss_inode journal */
        __u8 isjourn;
        void *journal;
        u64 jsize;
        void       *journal_base_addr;
        struct mutex journal_mutex;
        uint32_t    next_transaction_id;

        /* per crfss_inode active journal */
        void *trans;

        struct kmem_cache *trans_cachep;
        __u8 cachep_init;

         /* radix tree of all pages */
        struct radix_tree_root  page_tree;      

        /* devfs inode number */
        unsigned long i_ino;    

        /* disk physical address */
        unsigned long pi_addr;

        struct radix_tree_root dentry_tree;
        __u8 dentry_tree_init;
        unsigned long log_pages;

        struct dentry *dentry;

        /* per file pointer queue */
        unsigned int rd_nr;
        struct crfss_fstruct *per_rd_queue[MAX_FP_QSIZE];

        /* radix tree of pages in submission queue */
        int sq_tree_init;
        struct radix_tree_root sq_tree;
        struct rb_root sq_it_tree; 
        spinlock_t sq_tree_lock;
};

static inline struct crfs_sb_info *PMFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct crfs_inode_vfs *PMFS_I(struct inode *inode)
{
	return container_of(inode, struct crfs_inode_vfs, vfs_inode);
}

/* If this is part of a read-modify-write of the super block,
 * crfs_memunlock_super() before calling! */
static inline struct crfs_super_block *crfs_get_super(struct super_block *sb)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);

	return (struct crfs_super_block *)sbi->virt_addr;
}

static inline crfs_journal_t *crfs_get_journal(struct super_block *sb)
{
	struct crfs_super_block *ps = crfs_get_super(sb);

	return (crfs_journal_t *)((char *)ps +
			le64_to_cpu(ps->s_journal_offset));
}

static inline struct crfs_inode *crfs_get_inode_table(struct super_block *sb)
{
	struct crfs_super_block *ps = crfs_get_super(sb);

	return (struct crfs_inode *)((char *)ps +
			le64_to_cpu(ps->s_inode_table_offset));
}

static inline struct crfs_super_block *crfs_get_redund_super(struct super_block *sb)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);

	return (struct crfs_super_block *)(sbi->virt_addr + PMFS_SB_SIZE);
}

/* If this is part of a read-modify-write of the block,
 * crfs_memunlock_block() before calling! */
static inline void *crfs_get_block(struct super_block *sb, u64 block)
{
	struct crfs_super_block *ps = crfs_get_super(sb);

	return block ? ((void *)ps + block) : NULL;
}

/* uses CPU instructions to atomically write up to 8 bytes */
static inline void crfs_memcpy_atomic (void *dst, const void *src, u8 size)
{
	switch (size) {
		case 1: {
			volatile u8 *daddr = dst;
			const u8 *saddr = src;
			*daddr = *saddr;
			break;
		}
		case 2: {
			volatile u16 *daddr = dst;
			const u16 *saddr = src;
			*daddr = cpu_to_le16(*saddr);
			break;
		}
		case 4: {
			volatile u32 *daddr = dst;
			const u32 *saddr = src;
			*daddr = cpu_to_le32(*saddr);
			break;
		}
		case 8: {
			volatile u64 *daddr = dst;
			const u64 *saddr = src;
			*daddr = cpu_to_le64(*saddr);
			break;
		}
		default:
			crfs_dbg("error: memcpy_atomic called with %d bytes\n", size);
			//BUG();
	}
}

static inline void crfs_update_time_and_size(struct inode *inode,
	struct crfs_inode *pi)
{
	uint32_t words[2];
	/* pi->i_size, pi->i_ctime, and pi->i_mtime need to be atomically updated.
 	* So use cmpxchg16b here. */
	words[0] = cpu_to_le32(inode->i_ctime.tv_sec);
	words[1] = cpu_to_le32(inode->i_mtime.tv_sec);
	/* TODO: the following function assumes cmpxchg16b instruction writes
 	* 16 bytes atomically. Confirm if it is really true. */
	cmpxchg_double_local(&pi->i_size, (u64 *)&pi->i_ctime, pi->i_size,
		*(u64 *)&pi->i_ctime, inode->i_size, *(u64 *)words);
}

/* assumes the length to be 4-byte aligned */
static inline void memset_nt(void *dest, uint32_t dword, size_t length)
{
	uint64_t dummy1, dummy2;
	uint64_t qword = ((uint64_t)dword << 32) | dword;

	asm volatile ("movl %%edx,%%ecx\n"
		"andl $63,%%edx\n"
		"shrl $6,%%ecx\n"
		"jz 9f\n"
		"1:      movnti %%rax,(%%rdi)\n"
		"2:      movnti %%rax,1*8(%%rdi)\n"
		"3:      movnti %%rax,2*8(%%rdi)\n"
		"4:      movnti %%rax,3*8(%%rdi)\n"
		"5:      movnti %%rax,4*8(%%rdi)\n"
		"8:      movnti %%rax,5*8(%%rdi)\n"
		"7:      movnti %%rax,6*8(%%rdi)\n"
		"8:      movnti %%rax,7*8(%%rdi)\n"
		"leaq 64(%%rdi),%%rdi\n"
		"decl %%ecx\n"
		"jnz 1b\n"
		"9:     movl %%edx,%%ecx\n"
		"andl $7,%%edx\n"
		"shrl $3,%%ecx\n"
		"jz 11f\n"
		"10:     movnti %%rax,(%%rdi)\n"
		"leaq 8(%%rdi),%%rdi\n"
		"decl %%ecx\n"
		"jnz 10b\n"
		"11:     movl %%edx,%%ecx\n"
		"shrl $2,%%ecx\n"
		"jz 12f\n"
		"movnti %%eax,(%%rdi)\n"
		"12:\n"
		: "=D"(dummy1), "=d" (dummy2) : "D" (dest), "a" (qword), "d" (length) : "memory", "rcx");
}

static inline u64 __crfs_find_data_block(struct super_block *sb,
		struct crfs_inode *pi, unsigned long blocknr)
{
	u64 *level_ptr, bp = 0;
	u32 height, bit_shift;
	unsigned int idx;

	height = pi->height;
	bp = le64_to_cpu(pi->root);

	while (height > 0) {
		level_ptr = crfs_get_block(sb, bp);
		bit_shift = (height - 1) * META_BLK_SHIFT;
		idx = blocknr >> bit_shift;
		bp = le64_to_cpu(level_ptr[idx]);
		if (bp == 0)
			return 0;
		blocknr = blocknr & ((1 << bit_shift) - 1);
		height--;
	}
	return bp;
}

static inline unsigned int crfs_inode_blk_shift (struct crfs_inode *pi)
{
	return blk_type_to_shift[pi->i_blk_type];
}

static inline uint32_t crfs_inode_blk_size (struct crfs_inode *pi)
{
	return blk_type_to_size[pi->i_blk_type];
}

/* If this is part of a read-modify-write of the inode metadata,
 * crfs_memunlock_inode() before calling! */
static inline struct crfs_inode *crfs_get_inode(struct super_block *sb,
						  u64	ino)
{
	struct crfs_super_block *ps = crfs_get_super(sb);
	struct crfs_inode *inode_table = crfs_get_inode_table(sb);
	u64 bp, block, ino_offset;

	if (ino == 0)
		return NULL;

	block = ino >> crfs_inode_blk_shift(inode_table);
	bp = __crfs_find_data_block(sb, inode_table, block);

	if (bp == 0)
		return NULL;
	ino_offset = (ino & (crfs_inode_blk_size(inode_table) - 1));
	return (struct crfs_inode *)((void *)ps + bp + ino_offset);
}

static inline u64
crfs_get_addr_off(struct crfs_sb_info *sbi, void *addr)
{
	PMFS_ASSERT((addr >= sbi->virt_addr) &&
			(addr < (sbi->virt_addr + sbi->initsize)));
	return (u64)(addr - sbi->virt_addr);
}

static inline u64
crfs_get_block_off(struct super_block *sb, unsigned long blocknr,
		    unsigned short btype)
{
	return (u64)blocknr << PAGE_SHIFT;
}

static inline unsigned long
crfs_get_numblocks(unsigned short btype)
{
	unsigned long num_blocks;

	if (btype == PMFS_BLOCK_TYPE_4K) {
		num_blocks = 1;
	} else if (btype == PMFS_BLOCK_TYPE_2M) {
		num_blocks = 512;
	} else {
		//btype == PMFS_BLOCK_TYPE_1G 
		num_blocks = 0x40000;
	}
	return num_blocks;
}

static inline unsigned long
crfs_get_blocknr(struct super_block *sb, u64 block, unsigned short btype)
{
	return block >> PAGE_SHIFT;
}

static inline unsigned long crfs_get_pfn(struct super_block *sb, u64 block)
{
	return (PMFS_SB(sb)->phys_addr + block) >> PAGE_SHIFT;
}

static inline int crfs_is_mounting(struct super_block *sb)
{
	struct crfs_sb_info *sbi = (struct crfs_sb_info *)sb->s_fs_info;
	return sbi->s_mount_opt & PMFS_MOUNT_MOUNTING;
}

static inline struct crfs_inode_truncate_item * crfs_get_truncate_item (struct 
		super_block *sb, u64 ino)
{
	struct crfs_inode *pi = crfs_get_inode(sb, ino);
	return (struct crfs_inode_truncate_item *)(pi + 1);
}

static inline struct crfs_inode_truncate_item * crfs_get_truncate_list_head (
		struct super_block *sb)
{
	struct crfs_inode *pi = crfs_get_inode_table(sb);
	return (struct crfs_inode_truncate_item *)(pi + 1);
}

static inline void check_eof_blocks(struct super_block *sb, 
		struct crfs_inode *pi, loff_t size)
{
	if ((pi->i_flags & cpu_to_le32(PMFS_EOFBLOCKS_FL)) &&
		(size + sb->s_blocksize) > (le64_to_cpu(pi->i_blocks)
			<< sb->s_blocksize_bits))
		pi->i_flags &= cpu_to_le32(~PMFS_EOFBLOCKS_FL);
}

/*
 * Inodes and files operations
 */

/* dir.c */
extern const struct file_operations crfs_dir_operations;

/* file.c */
extern const struct inode_operations crfs_file_inode_operations;
extern const struct file_operations crfs_xip_file_operations;

/* inode.c */
extern const struct address_space_operations crfs_aops_xip;

/* bbuild.c */
void crfs_save_blocknode_mappings(struct super_block *sb);

/* namei.c */
extern const struct inode_operations crfs_dir_inode_operations;
extern const struct inode_operations crfs_special_inode_operations;

/* symlink.c */
extern const struct inode_operations crfs_symlink_inode_operations;

extern struct backing_dev_info crfs_backing_dev_info;

int crfs_check_integrity(struct super_block *sb,
	struct crfs_super_block *super);
void *crfs_ioremap(struct super_block *sb, phys_addr_t phys_addr,
	ssize_t size);

/* Emulated persistence APIs */
void crfs_set_backing_file(char *file_str);
void crfs_set_backing_option(int option);
void crfs_load_from_file(struct super_block *sb);
void crfs_store_to_file(struct super_block *sb);

int crfs_check_dir_entry(const char *function, struct inode *dir,
			  struct crfs_direntry *de, u8 *base,
			  unsigned long offset);

static inline int crfs_match(int len, const char *const name,
			      struct crfs_direntry *de)
{
	if (len == de->name_len && de->ino && !memcmp(de->name, name, len))
		return 1;
	return 0;
}

int crfs_search_dirblock(u8 *blk_base, struct inode *dir, struct qstr *child,
			  unsigned long offset,
			  struct crfs_direntry **res_dir,
			  struct crfs_direntry **prev_dir);

#endif /* __PMFS_H */
