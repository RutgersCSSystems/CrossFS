#include <linux/devfs.h>
#include <linux/buffer_head.h>
#include <linux/bitops.h>
#include <linux/sched.h>

static DEFINE_SPINLOCK(bitmap_lock);


/*
 * bitmap consists of blocks filled with 16bit words
 * bit set == busy, bit clear == free
 * endianness is a mess, but for counting zero bits it really doesn't matter...
 */
static __u32 count_free(struct buffer_head *map[], unsigned blocksize, __u32 numbits)
{
	__u32 sum = 0;
	unsigned blocks = DIV_ROUND_UP(numbits, blocksize * 8);

	while (blocks--) {
		unsigned words = blocksize / 2;
		__u16 *p = (__u16 *)(*map++)->b_data;
		while (words--)
			sum += 16 - hweight16(*p++);
	}

	return sum;
}

#if 0
void crfss_free_block(struct inode *inode, unsigned long block)
{
	struct super_block *sb = inode->i_sb;
	struct crfss_sb_info *sbi = crfss_sb(sb);
	struct buffer_head *bh;
	int k = sb->s_blocksize_bits + 3;
	unsigned long bit, zone;

	if (block < sbi->s_firstdatazone || block >= sbi->s_nzones) {
		printk("Trying to free block not in datazone\n");
		return;
	}
	zone = block - sbi->s_firstdatazone + 1;
	bit = zone & ((1<<k) - 1);
	zone >>= k;
	if (zone >= sbi->s_zmap_blocks) {
		printk("minix_free_block: nonexistent bitmap buffer\n");
		return;
	}
	bh = sbi->s_zmap[zone];
	spin_lock(&bitmap_lock);
	if (!minix_test_and_clear_bit(bit, bh->b_data))
		printk("minix_free_block (%s:%lu): bit already cleared\n",
		       sb->s_id, block);
	spin_unlock(&bitmap_lock);
	mark_buffer_dirty(bh);
	return;
}

#endif

/*
 * big-endian 16bit indexed bitmaps
 */

static inline int crfss_find_first_zero_bit(const void *vaddr, unsigned size)
{
        const unsigned short *p = vaddr, *addr = vaddr;
        unsigned short num;

        if (!size)
                return 0;

        size >>= 4;
        while (*p++ == 0xffff) {
                if (--size == 0)
                        return (p - addr) << 4;
        }

        num = *--p;
        return ((p - addr) << 4) + ffz(num);
}

#if 0
int crfss_new_block(struct inode * inode)
{
	struct crfss_sb_info *sbi = crfss_sb(inode->i_sb);
	int bits_per_zone = 8 * inode->i_sb->s_blocksize;
	int i;

	for (i = 0; i < sbi->s_zmap_blocks; i++) {
		struct buffer_head *bh = sbi->s_zmap[i];
		int j;

		spin_lock(&bitmap_lock);
		j = crfss_find_first_zero_bit(bh->b_data, bits_per_zone);
		if (j < bits_per_zone) {
			minix_set_bit(j, bh->b_data);
			spin_unlock(&bitmap_lock);
			mark_buffer_dirty(bh);
			j += i * bits_per_zone + sbi->s_firstdatazone-1;
			if (j < sbi->s_firstdatazone || j >= sbi->s_nzones)
				break;
			return j;
		}
		spin_unlock(&bitmap_lock);
	}
	return 0;
}

unsigned long crfss_count_free_blocks(struct super_block *sb)
{
	struct crfss_sb_info *sbi = crfss_sb(sb);
	u32 bits = sbi->s_nzones - (sbi->s_firstdatazone + 1);

	return (count_free(sbi->s_zmap, sb->s_blocksize, bits)
		<< sbi->s_log_zone_size);
}
#endif

struct crfss_inode *
crfss_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)
{
	int block;
	struct crfss_sb_info *sbi = crfss_sb(sb);
	struct crfss_inode *p;

	if (!ino || ino > sbi->s_ninodes) {
		printk("Bad inode number on dev %s: %ld is out of range\n",
		       sb->s_id, (long)ino);
		return NULL;
	}
	ino--;
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks +
		 ino / DEVFS_INODES_PER_BLOCK;
	*bh = sb_bread(sb, block);
	if (!*bh) {
		printk("Unable to read inode block\n");
		return NULL;
	}
	p = (void *)(*bh)->b_data;
	return p + ino % DEVFS_INODES_PER_BLOCK;
}



/* Clear the link count and mode of a deleted inode on disk. */
static void crfss_clear_inode(struct inode *inode)
{
	struct buffer_head *bh = NULL;

	if (1) {
		struct crfss_inode *raw_inode;
		raw_inode = crfss_raw_inode(inode->i_sb, inode->i_ino, &bh);

		#if 0
		if (raw_inode) {
			raw_inode->i_nlinks = 0;
			raw_inode->i_mode = 0;
		}
		#endif
	} 
	if (bh) {
		mark_buffer_dirty(bh);
		brelse (bh);
	}
}


int crfss_free_inode(struct inode * inode)
{
	struct super_block *sb = inode->i_sb;
	struct crfss_sb_info *sbi = crfss_sb(inode->i_sb);
	struct buffer_head *bh;
	int k = sb->s_blocksize_bits + 3;
	unsigned long ino, bit;

	ino = inode->i_ino;
	if (ino < 1 || ino > sbi->s_ninodes) {
		printk("%s:%d: inode 0 or nonexistent inode %lu\n",
			__FUNCTION__,__LINE__, ino);
		goto err_free_inode;
	}
	bit = ino & ((1<<k) - 1);
	ino >>= k;
	if (ino >= sbi->s_imap_blocks) {
		printk("%s:%d: nonexistent imap in superblock\n",
			__FUNCTION__,__LINE__);
		goto err_free_inode;
	}

#if defined(_DEVFS_DEBUG_BITMAP_DISK)
	crfss_clear_inode(inode);	/* clear on-disk copy */
#endif

	bh = sbi->s_imap[ino];

	spin_lock(&bitmap_lock);

	clear_bit(bit, bh->b_data);

	/*	printk("%s:%d: bit %lu already cleared\n", 
			__FUNCTION__,__LINE__, bit);
	}else*/
	printk("%s:%d: Clearing bit %lu\n", 
			__FUNCTION__,__LINE__, bit);

	spin_unlock(&bitmap_lock);
	//mark_buffer_dirty(bh);

	return 0;

err_free_inode:
	return -1;
}



int crfss_set_sb_bit(struct super_block *sb)
{
	struct crfss_sb_info *sbi = crfss_sb(sb);
	struct buffer_head * bh;
	int bits_per_zone = 8 * sb->s_blocksize;
	unsigned long j;
	int i;

	j = bits_per_zone;

#if defined(_DEVFS_DEBUG_BITMAP)
	printk(KERN_ALERT "%s:%d bits_per_zone %d \n", 
			__FUNCTION__,__LINE__, bits_per_zone);
#endif
	bh = NULL;

	spin_lock(&bitmap_lock);

	for (i = 0; i < sbi->s_imap_blocks; i++) {
		bh = sbi->s_imap[i];
		j = crfss_find_first_zero_bit(bh->b_data, bits_per_zone);

		if (j < bits_per_zone) {
			break;
		}
	}
	if (!bh || j >= bits_per_zone) {
		spin_unlock(&bitmap_lock);
		goto setbit_err;
	}
#if defined(_DEVFS_DEBUG_BITMAP)
	printk(KERN_ALERT "%s:%d bits_per_zone %d j %lu \n", 
			__FUNCTION__,__LINE__,bits_per_zone, j);
#endif
	set_bit(j, bh->b_data);
	spin_unlock(&bitmap_lock);
	//mark_buffer_dirty(bh);

	j += i * bits_per_zone;

	if (!j || j > sbi->s_ninodes) {

		printk("%s:%d err j %d, s_ninodes %lu \n",
			__FUNCTION__,__LINE__, j, sbi->s_ninodes);

		goto setbit_err;
	}
	return 0;
#if 0
	inode_init_owner(inode, dir, mode);
	inode->i_ino = j;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
	inode->i_blocks = 0;
	memset(&minix_i(inode)->u, 0, sizeof(minix_i(inode)->u));
	insert_inode_hash(inode);
	mark_inode_dirty(inode);
#endif
setbit_err:
	return -1;
}


struct inode* 
crfss_new_inode(struct super_block *sb, struct inode *inode, 
		umode_t mode, int *error)
{
	//struct super_block *sb = dir->i_sb;
	struct crfss_sb_info *sbi = crfss_sb(sb);
	//struct inode *inode = new_inode(sb);
	struct buffer_head * bh;
	int bits_per_zone = 8 * sb->s_blocksize;
	unsigned long j;
	int i;

	if (!inode) {
		*error = -ENOMEM;
		goto err_new_ino;
	}
	j = bits_per_zone;

#if defined(_DEVFS_DEBUG_BITMAP)
	printk(KERN_ALERT "%s:%d bits_per_zone %d inode %lu \n", 
		__FUNCTION__,__LINE__, bits_per_zone, inode->i_ino);
#endif
	bh = NULL;
	*error = -ENOSPC;

	spin_lock(&bitmap_lock);

	for (i = 0; i < sbi->s_imap_blocks; i++) {

		bh = sbi->s_imap[i];
		if(!bh) {	
			printk(KERN_ALERT "%s:%d s_imap NULL idx %d \n",
				__FUNCTION__,__LINE__, i);
			spin_unlock(&bitmap_lock);
			*error = -ENOMEM;
			goto err_new_ino;
		}

		j = crfss_find_first_zero_bit(bh->b_data, bits_per_zone);
		if (j < bits_per_zone) {
#if defined(_DEVFS_DEBUG_BITMAP)
			printk(KERN_ALERT "%s:%d bits_per_zone %d j %lu \n", 
				 __FUNCTION__,__LINE__, bits_per_zone, j);
#endif
			break;
		}
	}
	if (!bh || j >= bits_per_zone) {
		spin_unlock(&bitmap_lock);
		printk(KERN_ALERT "%s:%d j greater than bits_per_zone \n",
			__FUNCTION__,__LINE__);
		iput(inode);
		*error = -ENOSPC;
		goto err_new_ino;
	}
#if 0
	if (crfss_test_and_set_bit(j, bh->b_data)) {	/* shouldn't happen */
		spin_unlock(&bitmap_lock);
		printk("minix_new_inode: bit already set\n");
		iput(inode);
		return NULL;
	}
#endif

#if defined(_DEVFS_DEBUG_BITMAP)
	printk(KERN_ALERT "%s:%d bits_per_zone %d j %lu i %d\n", 
			__FUNCTION__,__LINE__,bits_per_zone, j, i);
#endif
	set_bit(j, bh->b_data);
	spin_unlock(&bitmap_lock);

	
        /*increment sb inode count */
        sbi->s_ninodes += 1;

	//mark_buffer_dirty(bh);
	j += i * bits_per_zone;

	if (!j || j > sbi->s_ninodes) {

		printk("%s:%d err j %d, s_ninodes %lu \n",
			__FUNCTION__,__LINE__, j, sbi->s_ninodes);

		goto err_new_ino;
	}

	inode->i_ino = j;	

#if defined(_DEVFS_DEBUG_BITMAP)
        printk(KERN_ALERT "%s:%d inode %lu\n",
		 __FUNCTION__,__LINE__, inode->i_ino);
#endif

	//iput(inode);

#if 0
	inode_init_owner(inode, dir, mode);
	inode->i_ino = j;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
	inode->i_blocks = 0;
	memset(&minix_i(inode)->u, 0, sizeof(minix_i(inode)->u));
	insert_inode_hash(inode);
	mark_inode_dirty(inode);
#endif
	*error = 0;
	return inode;

err_new_ino:
	return NULL;

}


unsigned long crfss_count_free_inodes(struct super_block *sb)
{
	struct crfss_sb_info *sbi = crfss_sb(sb);
	u32 bits = sbi->s_ninodes + 1;

	return count_free(sbi->s_imap, sb->s_blocksize, bits);
}
