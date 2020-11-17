#if 0
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/parser.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/magic.h>
#include <linux/exportfs.h>
#include <linux/random.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/devfs.h>
#include <linux/devfs_def.h>
#endif


#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/parser.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/magic.h>
#include <linux/exportfs.h>
#include <linux/random.h>
#include <linux/cred.h>
#include <linux/backing-dev.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include "pmfs.h"

#define _DEVFS_INITIALIZE 1

int measure_timing = 0;
int support_clwb = 0;
int support_pcommit = 0;
unsigned int crfss_dbgmask = 0;

static struct super_operations crfs_sops;
static const struct export_operations crfs_export_ops;
static struct kmem_cache *crfs_inode_cachep;
static struct kmem_cache *crfs_blocknode_cachep;
static struct kmem_cache *crfs_transaction_cachep;
/* FIXME: should the following variable be one per PMFS instance? */
unsigned int crfs_dbgmask = 0;

static void *qemu_vaddr = NULL;

#ifdef CONFIG_PMFS_TEST
static void *first_crfs_super;

struct crfs_super_block *get_crfs_super(void)
{
	return (struct crfs_super_block *)first_crfs_super;
}
EXPORT_SYMBOL(get_crfs_super);
#endif

void crfs_error_mng(struct super_block *sb, const char *fmt, ...)
{
	va_list args;

	printk("pmfs error: ");
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);

	if (test_opt(sb, ERRORS_PANIC))
		panic("pmfs: panic from previous error\n");
	if (test_opt(sb, ERRORS_RO)) {
		printk(KERN_CRIT "pmfs err: remounting filesystem read-only");
		sb->s_flags |= MS_RDONLY;
	}
}

static void crfs_set_blocksize(struct super_block *sb, unsigned long size)
{
	int bits;

	/*
	 * We've already validated the user input and the value here must be
	 * between PMFS_MAX_BLOCK_SIZE and PMFS_MIN_BLOCK_SIZE
	 * and it must be a power of 2.
	 */
	bits = fls(size) - 1;
	sb->s_blocksize_bits = bits;
	sb->s_blocksize = (1 << bits);
}

static inline int crfs_has_huge_ioremap(struct super_block *sb)
{
	struct crfs_sb_info *sbi = (struct crfs_sb_info *)sb->s_fs_info;

	return sbi->s_mount_opt & PMFS_MOUNT_HUGEIOREMAP;
}

void *crfs_ioremap(struct super_block *sb, phys_addr_t phys_addr, ssize_t size)
{
	void *retval;
	int protect, hugeioremap;

	if (sb) {
		protect = crfs_is_wprotected(sb);
		hugeioremap = crfs_has_huge_ioremap(sb);
	} else {
		protect = 0;
		hugeioremap = 1;
	}

	/*
	 * NOTE: Userland may not map this resource, we will mark the region so
	 * /dev/mem and the sysfs MMIO access will not be allowed. This
	 * restriction depends on STRICT_DEVMEM option. If this option is
	 * disabled or not available we mark the region only as busy.
	 */
	retval = request_mem_region_exclusive(phys_addr, size, "pmfs");
	if (!retval)
		goto fail;

	if (protect) {
		if (hugeioremap)
			retval = ioremap_hpage_cache_ro(phys_addr, size);
		else
			retval = ioremap_cache_ro(phys_addr, size);
	} else {
		if (hugeioremap)
			retval = ioremap_hpage_cache(phys_addr, size);
		else
			retval = ioremap_cache(phys_addr, size);
	}

fail:
	return retval;
}

static inline int crfs_iounmap(void *virt_addr, ssize_t size, int protected)
{
	iounmap(virt_addr);
	return 0;
}

static loff_t crfs_max_size(int bits)
{
	loff_t res;

	res = (1ULL << (3 * 9 + bits)) - 1;

	if (res > MAX_LFS_FILESIZE)
		res = MAX_LFS_FILESIZE;

	crfs_dbg_verbose("max file size %llu bytes\n", res);
	return res;
}

enum {
	Opt_addr, Opt_bpi, Opt_size, Opt_jsize,
	Opt_num_inodes, Opt_mode, Opt_uid,
	Opt_gid, Opt_blocksize, Opt_wprotect, Opt_wprotectold,
	Opt_err_cont, Opt_err_panic, Opt_err_ro,
	Opt_backing, Opt_backing_opt,
	Opt_hugemmap, Opt_nohugeioremap, Opt_dbgmask, Opt_err, Opt_vmmode
};

static const match_table_t tokens = {
	{ Opt_addr,	     "physaddr=%x"	  },
	{ Opt_bpi,	     "bpi=%u"		  },
	{ Opt_size,	     "init=%s"		  },
	{ Opt_jsize,     "jsize=%s"		  },
	{ Opt_num_inodes,"num_inodes=%u"  },
	{ Opt_mode,	     "mode=%o"		  },
	{ Opt_uid,	     "uid=%u"		  },
	{ Opt_gid,	     "gid=%u"		  },
	{ Opt_wprotect,	     "wprotect"		  },
	{ Opt_wprotectold,   "wprotectold"	  },
	{ Opt_err_cont,	     "errors=continue"	  },
	{ Opt_err_panic,     "errors=panic"	  },
	{ Opt_err_ro,	     "errors=remount-ro"  },
	{ Opt_backing,	     "backing=%s"	  },
	{ Opt_backing_opt,   "backing_opt=%u"	  },
	{ Opt_hugemmap,	     "hugemmap"		  },
	{ Opt_nohugeioremap, "nohugeioremap"	  },
	{ Opt_dbgmask,	     "dbgmask=%u"	  },
	{ Opt_vmmode,        "vmmode=%u"          }, 
	{ Opt_err,	     NULL		  },
};

phys_addr_t get_phys_addr(void **data)
{
	phys_addr_t phys_addr;
	char *options = (char *)*data;

	if (!options || strncmp(options, "physaddr=", 9) != 0)
		return (phys_addr_t)ULLONG_MAX;
	options += 9;
	phys_addr = (phys_addr_t)simple_strtoull(options, &options, 0);
	if (*options && *options != ',') {
		printk(KERN_ERR "Invalid phys addr specification: %s\n",
		       (char *)*data);
		return (phys_addr_t)ULLONG_MAX;
	}
	if (phys_addr & (PAGE_SIZE - 1)) {
		printk(KERN_ERR "physical address 0x%16llx for pmfs isn't "
		       "aligned to a page boundary\n", (u64)phys_addr);
		return (phys_addr_t)ULLONG_MAX;
	}
	if (*options == ',')
		options++;
	*data = (void *)options;
	return phys_addr;
}

static int crfs_parse_options(char *options, struct crfs_sb_info *sbi,
			       bool remount)
{
	char *p, *rest;
	substring_t args[MAX_OPT_ARGS];
	int option;

	if (!options)
		return 0;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case Opt_addr:
			if (remount)
				goto bad_opt;
			/* physaddr managed in get_phys_addr() */
			break;
		 case Opt_vmmode:                          
		         if (match_int(&args[0], &option)) 
                		 goto bad_val;             
		         sbi->vmmode = option;             
			 printk(KERN_ERR "sbi->vmmode %d %s:%d\n",
			               sbi->vmmode,__FUNCTION__,__LINE__);
		         break;                            
		case Opt_bpi:
			if (remount)
				goto bad_opt;
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->bpi = option;
			break;
		case Opt_uid:
			if (remount)
				goto bad_opt;
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->uid = make_kuid(current_user_ns(), option);
			break;
		case Opt_gid:
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->gid = make_kgid(current_user_ns(), option);
			break;
		case Opt_mode:
			if (match_octal(&args[0], &option))
				goto bad_val;
			sbi->mode = option & 01777U;
			break;
		case Opt_size:
			if (remount)
				goto bad_opt;
			/* memparse() will accept a K/M/G without a digit */
			if (!isdigit(*args[0].from))
				goto bad_val;
			sbi->initsize = memparse(args[0].from, &rest);
			set_opt(sbi->s_mount_opt, FORMAT);
			break;
		case Opt_jsize:
			if (remount)
				goto bad_opt;
			/* memparse() will accept a K/M/G without a digit */
			if (!isdigit(*args[0].from))
				goto bad_val;
			sbi->jsize = memparse(args[0].from, &rest);
			/* make sure journal size is integer power of 2 */
			if (sbi->jsize & (sbi->jsize - 1) ||
				sbi->jsize < PMFS_MINIMUM_JOURNAL_SIZE) {
				crfs_dbg("Invalid jsize: "
					"must be whole power of 2 & >= 64KB\n");
				goto bad_val;
			}
			break;
		case Opt_num_inodes:
			if (remount)
				goto bad_opt;
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->num_inodes = option;
			break;
		case Opt_err_panic:
			clear_opt(sbi->s_mount_opt, ERRORS_CONT);
			clear_opt(sbi->s_mount_opt, ERRORS_RO);
			set_opt(sbi->s_mount_opt, ERRORS_PANIC);
			break;
		case Opt_err_ro:
			clear_opt(sbi->s_mount_opt, ERRORS_CONT);
			clear_opt(sbi->s_mount_opt, ERRORS_PANIC);
			set_opt(sbi->s_mount_opt, ERRORS_RO);
			break;
		case Opt_err_cont:
			clear_opt(sbi->s_mount_opt, ERRORS_RO);
			clear_opt(sbi->s_mount_opt, ERRORS_PANIC);
			set_opt(sbi->s_mount_opt, ERRORS_CONT);
			break;
		case Opt_wprotect:
			if (remount)
				goto bad_opt;
			set_opt(sbi->s_mount_opt, PROTECT);
			crfs_info
				("PMFS: Enabling new Write Protection (CR0.WP)\n");
			break;
		case Opt_wprotectold:
			if (remount)
				goto bad_opt;
			set_opt(sbi->s_mount_opt, PROTECT_OLD);
			crfs_info
				("PMFS: Enabling old Write Protection (PAGE RW Bit)\n");
			break;
		case Opt_hugemmap:
			if (remount)
				goto bad_opt;
			set_opt(sbi->s_mount_opt, HUGEMMAP);
			crfs_info("PMFS: Enabling huge mappings for mmap\n");
			break;
		case Opt_nohugeioremap:
			if (remount)
				goto bad_opt;
			clear_opt(sbi->s_mount_opt, HUGEIOREMAP);
			crfs_info("PMFS: Disabling huge ioremap\n");
			break;
		case Opt_dbgmask:
			if (match_int(&args[0], &option))
				goto bad_val;
			crfs_dbgmask = option;
			break;
		case Opt_backing:
			strncpy(sbi->crfs_backing_file, args[0].from, 255);
			break;
		case Opt_backing_opt:
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->crfs_backing_option = option;
			break;
		default: {
			goto bad_opt;
		}
		}
	}

	return 0;

bad_val:
	printk(KERN_INFO "Bad value '%s' for mount option '%s'\n", args[0].from,
	       p);
	return -EINVAL;
bad_opt:
	printk(KERN_INFO "Bad mount option: \"%s\"\n", p);
	return -EINVAL;
}

static bool crfs_check_size (struct super_block *sb, unsigned long size)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	unsigned long minimum_size, num_blocks;

	/* space required for super block and root directory */
	minimum_size = 2 << sb->s_blocksize_bits;

	/* space required for inode table */
	if (sbi->num_inodes > 0)
		num_blocks = (sbi->num_inodes >>
			(sb->s_blocksize_bits - PMFS_INODE_BITS)) + 1;
	else
		num_blocks = 1;
	minimum_size += (num_blocks << sb->s_blocksize_bits);
	/* space required for journal */
	minimum_size += sbi->jsize;

	if (size < minimum_size)
	    return false;

	return true;
}


static struct crfs_inode *crfs_init(struct super_block *sb,
				      unsigned long size)
{
	unsigned long blocksize;
	u64 journal_meta_start, journal_data_start, inode_table_start;
	struct crfs_inode *root_i;
	struct crfs_super_block *super;
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	struct crfs_direntry *de;
	unsigned long blocknr;

	crfs_info("creating an empty pmfs of size %lu\n", size);
	if (!sbi->vmmode) {
		sbi->virt_addr = crfs_ioremap(sb, sbi->phys_addr, size);
	}
	sbi->block_start = (unsigned long)0;
	sbi->block_end = ((unsigned long)(size) >> PAGE_SHIFT);
	sbi->num_free_blocks = ((unsigned long)(size) >> PAGE_SHIFT);

	if (!sbi->virt_addr) {
		//printk(KERN_ERR "ioremap of the pmfs image failed(1)\n");
		//If ioremap failed, which will in VMs, allocated using kzalloc
		//TO Be removed 
		if (sbi->vmmode) {
			//sbi->virt_addr = kzalloc(size, GFP_KERNEL);
			if (qemu_vaddr == NULL) {
				sbi->virt_addr = vmalloc(size);
				qemu_vaddr = sbi->virt_addr;
			} else {
				sbi->virt_addr = qemu_vaddr;
			}
			if (!sbi->virt_addr) {
				printk(KERN_ERR "%s:%d sbi->virt_addr allocation failed\n", 
					 __FUNCTION__,__LINE__);
				return ERR_PTR(-EINVAL);
			}

		}
		if (!sbi->virt_addr)
			return ERR_PTR(-EINVAL);
	}
#ifdef CONFIG_PMFS_TEST
	if (!first_crfs_super)
		first_crfs_super = sbi->virt_addr;
#endif

	crfs_dbg_verbose("pmfs: Default block size set to 4K\n");
	blocksize = sbi->blocksize = PMFS_DEF_BLOCK_SIZE_4K;

	crfs_set_blocksize(sb, blocksize);
	blocksize = sb->s_blocksize;


	printk(KERN_ERR "%s:%d After blocksize = sb->s_blocksize;\n",
		 __FUNCTION__,__LINE__);

	if (sbi->blocksize && sbi->blocksize != blocksize)
		sbi->blocksize = blocksize;

	if (!crfs_check_size(sb, size)) {
		crfs_dbg("Specified PMFS size too small 0x%lx. Either increase"
			" PMFS size, or reduce num. of inodes (minimum 32)" 
			" or journal size (minimum 64KB)\n", size);
		return ERR_PTR(-EINVAL);
	}

	journal_meta_start = sizeof(struct crfs_super_block);
	journal_meta_start = (journal_meta_start + CACHELINE_SIZE - 1) &
		~(CACHELINE_SIZE - 1);
	inode_table_start = journal_meta_start + sizeof(crfs_journal_t);
	inode_table_start = (inode_table_start + CACHELINE_SIZE - 1) &
		~(CACHELINE_SIZE - 1);

	if ((inode_table_start + sizeof(struct crfs_inode)) > PMFS_SB_SIZE) {
		crfs_dbg("PMFS super block defined too small. defined 0x%x, "
				"required 0x%llx\n", PMFS_SB_SIZE,
			inode_table_start + sizeof(struct crfs_inode));
		return ERR_PTR(-EINVAL);
	}

	journal_data_start = PMFS_SB_SIZE * 2;
	journal_data_start = (journal_data_start + blocksize - 1) &
		~(blocksize - 1);

#if 1 //_ENABLE_BLOCKMAP
	/*printk(KERN_ERR "%s:%d sb->s_blocksize %d journal_data_start + sbi->jsize %u\n",
		__FUNCTION__,__LINE__, sb->s_blocksize, journal_data_start + sbi->jsize);*/
	crfs_init_blockmap(sb, journal_data_start + sbi->jsize);
	printk(KERN_ERR "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);
#endif
	crfs_dbg_verbose("journal meta start %llx data start 0x%llx, "
		"journal size 0x%x, inode_table 0x%llx\n", journal_meta_start,
		journal_data_start, sbi->jsize, inode_table_start);
	crfs_dbg_verbose("max file name len %d\n", (unsigned int)PMFS_NAME_LEN);

	super = crfs_get_super(sb);

        printk(KERN_ERR "%s:%d journal_data_start = PMFS_SB_SIZE * 2\n",
                 __FUNCTION__,__LINE__);

#ifdef _ENABLE_MEMLOCK
	crfs_memunlock_range(sb, super, journal_data_start);
#endif

	/* clear out super-block and inode table */
	memset_nt(super, 0, journal_data_start);
	super->s_size = cpu_to_le64(size);
	super->s_blocksize = cpu_to_le32(blocksize);
	super->s_magic = cpu_to_le16(PMFS_SUPER_MAGIC);
	super->s_journal_offset = cpu_to_le64(journal_meta_start);
	super->s_inode_table_offset = cpu_to_le64(inode_table_start);

        printk(KERN_ERR "%s:%d crfs_init_blockmap\n",__FUNCTION__,__LINE__);


#ifdef _ENABLE_MEMLOCK
	crfs_memlock_range(sb, super, journal_data_start);
#endif

#ifdef _ENABLE_PMFSJOURN
	if (crfs_journal_hard_init(sb, journal_data_start, sbi->jsize) < 0) {
		printk(KERN_ERR "Journal hard initialization failed\n");
		return ERR_PTR(-EINVAL);
	}
#endif
	printk(KERN_ERR "%s:%d Before crfs_init_inode_table\n",__FUNCTION__,__LINE__);

	if (crfs_init_inode_table(sb) < 0)
		return ERR_PTR(-EINVAL);

	printk(KERN_ERR "%s:%d After crfs_init_inode_table\n",__FUNCTION__,__LINE__);

	crfs_memunlock_range(sb, super, PMFS_SB_SIZE*2);
	crfs_sync_super(super);
	crfs_memlock_range(sb, super, PMFS_SB_SIZE*2);

	crfs_flush_buffer(super, PMFS_SB_SIZE, false);
	crfs_flush_buffer((char *)super + PMFS_SB_SIZE, sizeof(*super), false);

	crfs_new_block(sb, &blocknr, PMFS_BLOCK_TYPE_4K, 1);

	root_i = crfs_get_inode(sb, PMFS_ROOT_INO);

	crfs_memunlock_inode(sb, root_i);
	root_i->i_mode = cpu_to_le16(sbi->mode | S_IFDIR);
	root_i->i_uid = cpu_to_le32(from_kuid(&init_user_ns, sbi->uid));
	root_i->i_gid = cpu_to_le32(from_kgid(&init_user_ns, sbi->gid));
	root_i->i_links_count = cpu_to_le16(2);
	root_i->i_blk_type = PMFS_BLOCK_TYPE_4K;
	root_i->i_flags = 0;
	root_i->i_blocks = cpu_to_le32(1);
	root_i->i_size = cpu_to_le32(sb->s_blocksize);
	root_i->i_atime = root_i->i_mtime = root_i->i_ctime =
		cpu_to_le32(get_seconds());
	root_i->root = cpu_to_le64(crfs_get_block_off(sb, blocknr,
						       PMFS_BLOCK_TYPE_4K));
	root_i->height = 0;
	/* crfs_sync_inode(root_i); */
	crfs_memlock_inode(sb, root_i);
	crfs_flush_buffer(root_i, sizeof(*root_i), false);
	de = (struct crfs_direntry *)
		crfs_get_block(sb, crfs_get_block_off(sb, blocknr, PMFS_BLOCK_TYPE_4K));

	crfs_memunlock_range(sb, de, sb->s_blocksize);
	de->ino = cpu_to_le64(PMFS_ROOT_INO);
	de->name_len = 1;
	de->de_len = cpu_to_le16(PMFS_DIR_REC_LEN(de->name_len));
	strcpy(de->name, ".");
	de = (struct crfs_direntry *)((char *)de + le16_to_cpu(de->de_len));
	de->ino = cpu_to_le64(PMFS_ROOT_INO);
	de->de_len = cpu_to_le16(sb->s_blocksize - PMFS_DIR_REC_LEN(1));
	de->name_len = 2;
	strcpy(de->name, "..");
	crfs_memlock_range(sb, de, sb->s_blocksize);
	crfs_flush_buffer(de, PMFS_DIR_REC_LEN(2), false);
	PERSISTENT_MARK();
	PERSISTENT_BARRIER();
	return root_i;
}

static inline void set_default_opts(struct crfs_sb_info *sbi)
{
	/* set_opt(sbi->s_mount_opt, PROTECT); */
	set_opt(sbi->s_mount_opt, HUGEIOREMAP);
	set_opt(sbi->s_mount_opt, ERRORS_CONT);
	sbi->crfs_backing_file[0] = '\0';
	sbi->crfs_backing_option = 0;
	sbi->jsize = PMFS_DEFAULT_JOURNAL_SIZE;
}

static void crfs_root_check(struct super_block *sb, struct crfs_inode *root_pi)
{
/*
 *      if (root_pi->i_d.d_next) {
 *              crfs_warn("root->next not NULL, trying to fix\n");
 *              goto fail1;
 *      }
 */
	if (!S_ISDIR(le16_to_cpu(root_pi->i_mode)))
		crfs_warn("root is not a directory!\n");
#if 0
	if (crfs_calc_checksum((u8 *)root_pi, PMFS_INODE_SIZE)) {
		crfs_dbg("checksum error in root inode, trying to fix\n");
		goto fail3;
	}
#endif
}

int crfs_check_integrity(struct super_block *sb,
			  struct crfs_super_block *super)
{
	struct crfs_super_block *super_redund;

	super_redund =
		(struct crfs_super_block *)((char *)super + PMFS_SB_SIZE);

	/* Do sanity checks on the superblock */
	if (le16_to_cpu(super->s_magic) != PMFS_SUPER_MAGIC) {
		if (le16_to_cpu(super_redund->s_magic) != PMFS_SUPER_MAGIC) {
			printk(KERN_ERR "Can't find a valid pmfs partition\n");
			goto out;
		} else {
			crfs_warn
				("Error in super block: try to repair it with "
				"the redundant copy");
			/* Try to auto-recover the super block */
			if (sb)
				crfs_memunlock_super(sb, super);
			memcpy(super, super_redund,
				sizeof(struct crfs_super_block));
			if (sb)
				crfs_memlock_super(sb, super);
			crfs_flush_buffer(super, sizeof(*super), false);
			crfs_flush_buffer((char *)super + PMFS_SB_SIZE,
				sizeof(*super), false);

		}
	}

	/* Read the superblock */
	if (crfs_calc_checksum((u8 *)super, PMFS_SB_STATIC_SIZE(super))) {
		if (crfs_calc_checksum((u8 *)super_redund,
					PMFS_SB_STATIC_SIZE(super_redund))) {
			printk(KERN_ERR "checksum error in super block\n");
			goto out;
		} else {
			crfs_warn
				("Error in super block: try to repair it with "
				"the redundant copy");
			/* Try to auto-recover the super block */
			if (sb)
				crfs_memunlock_super(sb, super);
			memcpy(super, super_redund,
				sizeof(struct crfs_super_block));
			if (sb)
				crfs_memlock_super(sb, super);
			crfs_flush_buffer(super, sizeof(*super), false);
			crfs_flush_buffer((char *)super + PMFS_SB_SIZE,
				sizeof(*super), false);
		}
	}

	return 1;
out:
	return 0;
}

static void crfs_recover_truncate_list(struct super_block *sb)
{
	struct crfs_inode_truncate_item *head = crfs_get_truncate_list_head(sb);
	u64 ino_next = le64_to_cpu(head->i_next_truncate);
	struct crfs_inode *pi;
	struct crfs_inode_truncate_item *li;
	struct inode *inode;

	if (ino_next == 0)
		return;

	while (ino_next != 0) {
		pi = crfs_get_inode(sb, ino_next);
		li = (struct crfs_inode_truncate_item *)(pi + 1);
		inode = crfs_iget(sb, ino_next);
		if (IS_ERR(inode))
			break;
		crfs_dbg("Recover ino %llx nlink %d sz %llx:%llx\n", ino_next,
			inode->i_nlink, pi->i_size, li->i_truncatesize);
		if (inode->i_nlink) {
			/* set allocation hint */
			crfs_set_blocksize_hint(sb, pi, 
					le64_to_cpu(li->i_truncatesize));
			crfs_setsize(inode, le64_to_cpu(li->i_truncatesize));
			crfs_update_isize(inode, pi);
		} else {
			/* free the inode */
			crfs_dbg("deleting unreferenced inode %lx\n",
				inode->i_ino);
		}
		iput(inode);
		crfs_flush_buffer(pi, CACHELINE_SIZE, false);
		ino_next = le64_to_cpu(li->i_next_truncate);
	}
	PERSISTENT_MARK();
	PERSISTENT_BARRIER();
	/* reset the truncate_list */
	crfs_memunlock_range(sb, head, sizeof(*head));
	head->i_next_truncate = 0;
	crfs_memlock_range(sb, head, sizeof(*head));
	crfs_flush_buffer(head, sizeof(*head), false);
	PERSISTENT_MARK();
	PERSISTENT_BARRIER();
}

int crfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct crfs_super_block *super;
	struct crfs_inode *root_pi;
	struct crfs_sb_info *sbi = NULL;
	struct inode *root_i = NULL;
	unsigned long blocksize, initsize = 0;
	u32 random = 0;
	int retval = -EINVAL;

	BUILD_BUG_ON(sizeof(struct crfs_super_block) > PMFS_SB_SIZE);
	BUILD_BUG_ON(sizeof(struct crfs_inode) > PMFS_INODE_SIZE);

	sbi = kzalloc(sizeof(struct crfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;

	printk(KERN_ALERT "Before crfs_fill_super \n");

	set_default_opts(sbi);

	get_random_bytes(&random, sizeof(u32));
	atomic_set(&sbi->next_generation, random);

	printk(KERN_ALERT "After atomic_set \n");

	/* Init with default values */
	INIT_LIST_HEAD(&sbi->block_inuse_head);
	sbi->mode = (S_IRUGO | S_IXUGO | S_IWUSR);
	sbi->uid = current_fsuid();
	sbi->gid = current_fsgid();
	set_opt(sbi->s_mount_opt, XIP);
	clear_opt(sbi->s_mount_opt, PROTECT);
	set_opt(sbi->s_mount_opt, HUGEIOREMAP);

	INIT_LIST_HEAD(&sbi->s_truncate);
	mutex_init(&sbi->s_truncate_lock);
	mutex_init(&sbi->inode_table_mutex);
	mutex_init(&sbi->s_lock);
	mutex_init(&sbi->journal_mutex);

	printk(KERN_ALERT "Before crfs_parse_options \n");

	if (crfs_parse_options(data, sbi, 0))
		goto out;

	printk(KERN_ALERT "After crfs_parse_options \n");

	printk(KERN_ALERT "After crfs_fill_super %d\n", sbi->vmmode);

	sbi->phys_addr = get_phys_addr(&data);
	if (sbi->phys_addr == (phys_addr_t)ULLONG_MAX) {
		printk(KERN_ALERT "After crfs_fill_super %s:%d %llu\n",
				__FUNCTION__,__LINE__, sbi->phys_addr);
		if (!sbi->vmmode)
		    goto out;
	}

	printk(KERN_ALERT "After get_phys_addr %s:%d \n",
                                __FUNCTION__,__LINE__);



	set_opt(sbi->s_mount_opt, MOUNTING);
	initsize = sbi->initsize;

	/* Init a new pmfs instance */
	if (initsize) {
		if (qemu_vaddr == NULL) {
			printk(KERN_ERR "%s:%d Before crfs_init\n",__FUNCTION__,__LINE__);
			root_pi = crfs_init(sb, initsize);
			printk(KERN_ERR "%s:%d After crfs_init\n",__FUNCTION__,__LINE__);
			if (IS_ERR(root_pi))
				goto out;

			super = crfs_get_super(sb);

			goto setup_sb;
		} else {
			/* If qemu_vaddr is already set, then do not format the storage */
			sbi->virt_addr = qemu_vaddr;
			sbi->s_mount_opt &= (~PMFS_MOUNT_FORMAT);
		}
	} else {
		crfs_load_from_file(sb);
	}
	crfs_dbg_verbose("checking physical address 0x%016llx for pmfs image\n",
		  (u64)sbi->phys_addr);

#if 0
	printk(KERN_ERR "%s:%d Before crfs_ioremap\n",__FUNCTION__,__LINE__);

	/* Map only one page for now. Will remap it when fs size is known. */
	initsize = PAGE_SIZE;
	sbi->virt_addr = crfs_ioremap(sb, sbi->phys_addr, initsize);

	if (!sbi->virt_addr) {
		if (sbi->vmmode) {
		        sbi->virt_addr = kzalloc(initsize, GFP_KERNEL);
		 	if (!sbi->virt_addr) {
		                 printk(KERN_ERR "%s:%d sbi->virt_addr allocation failed\n", 
						__FUNCTION__,__LINE__);
                		  goto out;
			}
		}
		if (!sbi->virt_addr)
			printk(KERN_ERR "ioremap of the pmfs image failed(2)\n");

		goto out;
	}

	super = crfs_get_super(sb);

	initsize = le64_to_cpu(super->s_size);
	sbi->initsize = initsize;
	crfs_dbg_verbose("pmfs image appears to be %lu KB in size\n",
		   initsize >> 10);

	if (!sbi->vmmode) {
		crfs_iounmap(sbi->virt_addr, PAGE_SIZE, crfs_is_wprotected(sb));
	}

        printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);
	/* Remap the whole filesystem now */
	if (!sbi->vmmode) {
		release_mem_region(sbi->phys_addr, PAGE_SIZE);
	}else {
		//kfree(sbi->virt_addr);
		vfree(sbi->virt_addr);
	}

	printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);

	/* FIXME: Remap the whole filesystem in pmfs virtual address range. */
	sbi->virt_addr = crfs_ioremap(sb, sbi->phys_addr, initsize);

	if (!sbi->virt_addr) {

	        printk(KERN_ERR "%s:%d sbi->virt_addr \n",__FUNCTION__,__LINE__);

		if (sbi->vmmode) {
		        sbi->virt_addr = kzalloc(initsize, GFP_KERNEL);
		 	if (!sbi->virt_addr) {
		                 printk(KERN_ERR "%s:%d sbi->virt_addr allocation failed\n", 
						__FUNCTION__,__LINE__);
                		  goto out;
			}
		}

		if (!sbi->virt_addr) {
			printk(KERN_ERR "%s:%d ioremap of the pmfs image failed(3)\n", 
					__FUNCTION__,__LINE__);
		}

		goto out;
	}

        printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);
#endif

	if (!crfs_check_size(sb, initsize)) {
		printk(KERN_ALERT "size not correct!\n");
		retval = -EINVAL;
		goto out;
	}

	super = crfs_get_super(sb);

	if (crfs_journal_soft_init(sb)) {
		retval = -EINVAL;
		printk(KERN_ERR "Journal initialization failed\n");
		goto out;
	}
	if (crfs_recover_journal(sb)) {
		retval = -EINVAL;
		printk(KERN_ERR "Journal recovery failed\n");
		goto out;
	}

	if (crfs_check_integrity(sb, super) == 0) {
		crfs_dbg("Memory contains invalid pmfs %x:%x\n",
				le16_to_cpu(super->s_magic), PMFS_SUPER_MAGIC);
		goto out;
	}

	blocksize = le32_to_cpu(super->s_blocksize);
	crfs_set_blocksize(sb, blocksize);

	crfs_dbg_verbose("blocksize %lu\n", blocksize);

	printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);

	/* Read the root inode */
	root_pi = crfs_get_inode(sb, PMFS_ROOT_INO);

	printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);	

	/* Check that the root inode is in a sane state */
	crfs_root_check(sb, root_pi);

#ifdef CONFIG_PMFS_TEST
	if (!first_crfs_super)
		first_crfs_super = sbi->virt_addr;
#endif

	/* Set it all up.. */
setup_sb:
	printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);
	sb->s_magic = le16_to_cpu(super->s_magic);
	sb->s_op = &crfs_sops;
	sb->s_maxbytes = crfs_max_size(sb->s_blocksize_bits);
	sb->s_time_gran = 1;
	sb->s_export_op = &crfs_export_ops;
	sb->s_xattr = NULL;
	sb->s_flags |= MS_NOSEC;
	root_i = crfs_iget(sb, PMFS_ROOT_INO);
	if (IS_ERR(root_i)) {
		retval = PTR_ERR(root_i);
		goto out;
	}
	printk(KERN_ERR "%s:%d \n",__FUNCTION__,__LINE__);

	sb->s_root = d_make_root(root_i);
	if (!sb->s_root) {
		printk(KERN_ERR "get pmfs root inode failed\n");
		retval = -ENOMEM;
		goto out;
	}

	crfs_recover_truncate_list(sb);
	/* If the FS was not formatted on this mount, scan the meta-data after
	 * truncate list has been processed */
	if ((sbi->s_mount_opt & PMFS_MOUNT_FORMAT) == 0)
		crfs_setup_blocknode_map(sb);

	if (!(sb->s_flags & MS_RDONLY)) {
		u64 mnt_write_time;
		/* update mount time and write time atomically. */
		mnt_write_time = (get_seconds() & 0xFFFFFFFF);
		mnt_write_time = mnt_write_time | (mnt_write_time << 32);

		crfs_memunlock_range(sb, &super->s_mtime, 8);
		crfs_memcpy_atomic(&super->s_mtime, &mnt_write_time, 8);
		crfs_memlock_range(sb, &super->s_mtime, 8);

		crfs_flush_buffer(&super->s_mtime, 8, false);
		PERSISTENT_MARK();
		PERSISTENT_BARRIER();
	}

	clear_opt(sbi->s_mount_opt, MOUNTING);
	retval = 0;
	return retval;
out:
	if (sbi->virt_addr && !sbi->vmmode) {
		crfs_iounmap(sbi->virt_addr, initsize, crfs_is_wprotected(sb));
		release_mem_region(sbi->phys_addr, initsize);
	}

	kfree(sbi);
	return retval;
}

int crfs_statfs(struct dentry *d, struct kstatfs *buf)
{
	struct super_block *sb = d->d_sb;
	unsigned long count = 0;
	struct crfs_sb_info *sbi = (struct crfs_sb_info *)sb->s_fs_info;

	buf->f_type = PMFS_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;

	count = sbi->block_end;
	buf->f_blocks = sbi->block_end;
	buf->f_bfree = buf->f_bavail = crfs_count_free_blocks(sb);
	buf->f_files = (sbi->s_inodes_count);
	buf->f_ffree = (sbi->s_free_inodes_count);
	buf->f_namelen = PMFS_NAME_LEN;
	crfs_dbg_verbose("crfs_stats: total 4k free blocks 0x%llx\n",
		buf->f_bfree);
	crfs_dbg_verbose("total inodes 0x%x, free inodes 0x%x, "
		"blocknodes 0x%lx\n", (sbi->s_inodes_count),
		(sbi->s_free_inodes_count), (sbi->num_blocknode_allocated));
	return 0;
}

static int crfs_show_options(struct seq_file *seq, struct dentry *root)
{
	struct crfs_sb_info *sbi = PMFS_SB(root->d_sb);

	seq_printf(seq, ",physaddr=0x%016llx", (u64)sbi->phys_addr);
	if (sbi->initsize)
		seq_printf(seq, ",init=%luk", sbi->initsize >> 10);
	if (sbi->blocksize)
		seq_printf(seq, ",bs=%lu", sbi->blocksize);
	if (sbi->bpi)
		seq_printf(seq, ",bpi=%lu", sbi->bpi);
	if (sbi->num_inodes)
		seq_printf(seq, ",N=%lu", sbi->num_inodes);
	if (sbi->mode != (S_IRWXUGO | S_ISVTX))
		seq_printf(seq, ",mode=%03o", sbi->mode);
	if (uid_valid(sbi->uid))
		seq_printf(seq, ",uid=%u", from_kuid(&init_user_ns, sbi->uid));
	if (gid_valid(sbi->gid))
		seq_printf(seq, ",gid=%u", from_kgid(&init_user_ns, sbi->gid));
	if (test_opt(root->d_sb, ERRORS_RO))
		seq_puts(seq, ",errors=remount-ro");
	if (test_opt(root->d_sb, ERRORS_PANIC))
		seq_puts(seq, ",errors=panic");
	/* memory protection disabled by default */
	if (test_opt(root->d_sb, PROTECT))
		seq_puts(seq, ",wprotect");
	if (test_opt(root->d_sb, HUGEMMAP))
		seq_puts(seq, ",hugemmap");
	if (test_opt(root->d_sb, HUGEIOREMAP))
		seq_puts(seq, ",hugeioremap");
	/* xip not enabled by default */
	if (test_opt(root->d_sb, XIP))
		seq_puts(seq, ",xip");

	return 0;
}

int crfs_remount(struct super_block *sb, int *mntflags, char *data)
{
	unsigned long old_sb_flags;
	unsigned long old_mount_opt;
	struct crfs_super_block *ps;
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	int ret = -EINVAL;

	/* Store the old options */
	mutex_lock(&sbi->s_lock);
	old_sb_flags = sb->s_flags;
	old_mount_opt = sbi->s_mount_opt;

	if (crfs_parse_options(data, sbi, 1))
		goto restore_opt;

	sb->s_flags = (sb->s_flags & ~MS_POSIXACL) |
		      ((sbi->s_mount_opt & PMFS_MOUNT_POSIX_ACL) ? MS_POSIXACL : 0);

	if ((*mntflags & MS_RDONLY) != (sb->s_flags & MS_RDONLY)) {
		u64 mnt_write_time;
		ps = crfs_get_super(sb);
		/* update mount time and write time atomically. */
		mnt_write_time = (get_seconds() & 0xFFFFFFFF);
		mnt_write_time = mnt_write_time | (mnt_write_time << 32);

		crfs_memunlock_range(sb, &ps->s_mtime, 8);
		crfs_memcpy_atomic(&ps->s_mtime, &mnt_write_time, 8);
		crfs_memlock_range(sb, &ps->s_mtime, 8);

		crfs_flush_buffer(&ps->s_mtime, 8, false);
		PERSISTENT_MARK();
		PERSISTENT_BARRIER();
	}

	mutex_unlock(&sbi->s_lock);
	ret = 0;
	return ret;

restore_opt:
	sb->s_flags = old_sb_flags;
	sbi->s_mount_opt = old_mount_opt;
	mutex_unlock(&sbi->s_lock);
	return ret;
}

void crfs_put_super(struct super_block *sb)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	struct crfs_super_block *ps = crfs_get_super(sb);
	u64 size = le64_to_cpu(ps->s_size);
	struct crfs_blocknode *i;
	struct list_head *head = &(sbi->block_inuse_head);

#ifdef CONFIG_PMFS_TEST
	if (first_crfs_super == sbi->virt_addr)
		first_crfs_super = NULL;
#endif

	/* It's unmount time, so unmap the pmfs memory */
	if (sbi->virt_addr) {
		crfs_save_blocknode_mappings(sb);
		crfs_journal_uninit(sb);
		crfs_store_to_file(sb);
		crfs_iounmap(sbi->virt_addr, size, crfs_is_wprotected(sb));
		sbi->virt_addr = NULL;
		release_mem_region(sbi->phys_addr, size);
	}

	/* Free all the crfs_blocknodes */
	while (!list_empty(head)) {
		i = list_first_entry(head, struct crfs_blocknode, link);
		list_del(&i->link);
		crfs_free_blocknode(sb, i);
	}
	sb->s_fs_info = NULL;
	crfs_dbgmask = 0;
	kfree(sbi);
}

inline void crfs_free_transaction(crfs_transaction_t *trans)
{
	kmem_cache_free(crfs_transaction_cachep, trans);
}

void __crfs_free_blocknode(struct crfs_blocknode *bnode)
{
	kmem_cache_free(crfs_blocknode_cachep, bnode);
}

void crfs_free_blocknode(struct super_block *sb, struct crfs_blocknode *bnode)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	sbi->num_blocknode_allocated--;
	__crfs_free_blocknode(bnode);
}

inline crfs_transaction_t *crfs_alloc_transaction(void)
{
	return (crfs_transaction_t *)
		kmem_cache_alloc(crfs_transaction_cachep, GFP_NOFS);
}

#if 0
struct crfs_blocknode *crfs_alloc_blocknode(struct super_block *sb)
{
	struct crfs_blocknode *p;
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	p = (struct crfs_blocknode *)
		kmem_cache_alloc(crfs_blocknode_cachep, GFP_NOFS);
	if (p) {
		sbi->num_blocknode_allocated++;
	}
	return p;
}
#endif

static struct inode *crfs_alloc_inode(struct super_block *sb)
{
	struct crfs_inode_vfs *vi = (struct crfs_inode_vfs *)
				     kmem_cache_alloc(crfs_inode_cachep, GFP_NOFS);

	if (!vi)
		return NULL;
	vi->vfs_inode.i_version = 1;
 
	// The rest code initialize
	// devfs specific fields
        vi->dentry_tree_init = 0; 
        vi->isjourn = 0; 
        vi->cachep_init = 0; 

        rwlock_init(&vi->i_meta_lock);
        INIT_RADIX_TREE(&vi->sq_tree, GFP_ATOMIC);
        __SPIN_LOCK_UNLOCKED(vi->sq_tree_lock);
        vi->sq_tree_init = _DEVFS_INITIALIZE;

        vi->rd_nr = 0;

	return &vi->vfs_inode;
}

static void crfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);

	kmem_cache_free(crfs_inode_cachep, PMFS_I(inode));
}

static void crfs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, crfs_i_callback);
}

static void init_once(void *foo)
{
	struct crfs_inode_vfs *vi = (struct crfs_inode_vfs *)foo;

	vi->i_dir_start_lookup = 0;
	INIT_LIST_HEAD(&vi->i_truncated);
	inode_init_once(&vi->vfs_inode);
}


int __init init_blocknode_cache(void)
{

	printk(KERN_ALERT "init_blocknode_cache %s:%d \n", 
			__FUNCTION__,__LINE__);

	crfs_blocknode_cachep = kmem_cache_create("crfs_blocknode_cache",
					sizeof(struct crfs_blocknode),
					0, (SLAB_RECLAIM_ACCOUNT |
                                        SLAB_MEM_SPREAD), NULL);

        printk(KERN_ALERT "init_blocknode_cache %s:%d \n",
                        __FUNCTION__,__LINE__);

	if (crfs_blocknode_cachep == NULL)
		return -ENOMEM;
	return 0;
}


static int __init init_inodecache(void)
{
	crfs_inode_cachep = kmem_cache_create("crfs_inode_cache",
					       sizeof(struct crfs_inode_vfs),
					       0, (SLAB_RECLAIM_ACCOUNT |
						   SLAB_MEM_SPREAD), init_once);
	if (crfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static int __init init_transaction_cache(void)
{
	crfs_transaction_cachep = kmem_cache_create("crfs_journal_transaction",
			sizeof(crfs_transaction_t), 0, (SLAB_RECLAIM_ACCOUNT |
			SLAB_MEM_SPREAD), NULL);
	if (crfs_transaction_cachep == NULL) {
		crfs_dbg("PMFS: failed to init transaction cache\n");
		return -ENOMEM;
	}
	return 0;
}

static void destroy_transaction_cache(void)
{
	if (crfs_transaction_cachep)
		kmem_cache_destroy(crfs_transaction_cachep);
	crfs_transaction_cachep = NULL;
}

static void destroy_inodecache(void)
{
	kmem_cache_destroy(crfs_inode_cachep);
}

static void destroy_blocknode_cache(void)
{
	kmem_cache_destroy(crfs_blocknode_cachep);
}

/*
 * the super block writes are all done "on the fly", so the
 * super block is never in a "dirty" state, so there's no need
 * for write_super.
 */
static struct super_operations crfs_sops = {
	.alloc_inode	= crfs_alloc_inode,
	.destroy_inode	= crfs_destroy_inode,
	.write_inode	= crfs_write_inode,
	.dirty_inode	= crfs_dirty_inode,
	.evict_inode	= crfs_evict_inode,
	.put_super	= crfs_put_super,
	.statfs		= crfs_statfs,
	.remount_fs	= crfs_remount,
	.show_options	= crfs_show_options,
};

static struct dentry *crfs_mount(struct file_system_type *fs_type,
				  int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, crfs_fill_super);
}

static struct file_system_type crfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "pmfs",
	.mount		= crfs_mount,
	.kill_sb	= kill_anon_super,
};

static struct inode *crfs_nfs_get_inode(struct super_block *sb,
					 u64 ino, u32 generation)
{
	struct crfs_sb_info *sbi = PMFS_SB(sb);
	struct inode *inode;

	if (ino < PMFS_ROOT_INO)
		return ERR_PTR(-ESTALE);

	if ((ino >> PMFS_INODE_BITS) > (sbi->s_inodes_count))
		return ERR_PTR(-ESTALE);

	inode = crfs_iget(sb, ino);
	if (IS_ERR(inode))
		return ERR_CAST(inode);

	if (generation && inode->i_generation != generation) {
		/* we didn't find the right inode.. */
		iput(inode);
		return ERR_PTR(-ESTALE);
	}

	return inode;
}

static struct dentry *crfs_fh_to_dentry(struct super_block *sb,
					 struct fid *fid, int fh_len,
					 int fh_type)
{
	return generic_fh_to_dentry(sb, fid, fh_len, fh_type,
				    crfs_nfs_get_inode);
}

static struct dentry *crfs_fh_to_parent(struct super_block *sb,
					 struct fid *fid, int fh_len,
					 int fh_type)
{
	return generic_fh_to_parent(sb, fid, fh_len, fh_type,
				    crfs_nfs_get_inode);
}

static const struct export_operations crfs_export_ops = {
	.fh_to_dentry	= crfs_fh_to_dentry,
	.fh_to_parent	= crfs_fh_to_parent,
	.get_parent	= crfs_get_parent,
};

static int __init init_crfs_fs(void)
{
	int rc = 0;

	printk(KERN_ALERT "Before init_blocknode_cache %s:%d \n",
                        __FUNCTION__,__LINE__);

	rc = init_blocknode_cache();
	if (rc)
		return rc;

	printk(KERN_ALERT "Before init_transaction_cache %s:%d \n",
                __FUNCTION__,__LINE__);

	rc = init_transaction_cache();
	if (rc)
		goto out1;


	printk(KERN_ALERT "Before init_inodecache %s:%d \n",
	        __FUNCTION__,__LINE__);

	rc = init_inodecache();
	if (rc)
		goto out2;

	 printk(KERN_ALERT "Before bdi_init %s:%d \n",
	         __FUNCTION__,__LINE__);

	rc = bdi_init(&crfs_backing_dev_info);
	if (rc)
		goto out3;

	 printk(KERN_ALERT "before register_filesystem %s:%d \n",
        	 __FUNCTION__,__LINE__);
	

	rc = register_filesystem(&crfs_fs_type);
	if (rc)
		goto out4;

	 printk(KERN_ALERT "after register_filesystem %s:%d \n",
        	 __FUNCTION__,__LINE__);

	return 0;

out4:
	bdi_destroy(&crfs_backing_dev_info);
out3:
	destroy_inodecache();
out2:
	destroy_transaction_cache();
out1:
	destroy_blocknode_cache();
	return rc;
}

static void __exit exit_crfs_fs(void)
{
	unregister_filesystem(&crfs_fs_type);
	bdi_destroy(&crfs_backing_dev_info);
	destroy_inodecache();
	destroy_blocknode_cache();
	destroy_transaction_cache();
}

MODULE_AUTHOR("Intel Corporation <linux-pmfs@intel.com>");
MODULE_DESCRIPTION("Persistent Memory File System");
MODULE_LICENSE("GPL");

module_init(init_crfs_fs)
module_exit(exit_crfs_fs)




















#if 0



int support_clwb = 0;
int support_pcommit = 0;

//static struct kmem_cache *crfss_inode_cachep;
static struct kmem_cache *crfss_range_node_cachep;
unsigned int crfss_dbgmask = 0;

#if 1
enum {
        Opt_addr, Opt_bpi, Opt_size, Opt_jsize,
        Opt_num_inodes, Opt_mode, Opt_uid,
        Opt_gid, Opt_blocksize, Opt_wprotect, Opt_wprotectold,
        Opt_err_cont, Opt_err_panic, Opt_err_ro,
        Opt_backing, Opt_backing_opt,
        Opt_hugemmap, Opt_nohugeioremap, Opt_dbgmask, Opt_vmmode, 
	Opt_dentry_size, Opt_inodeoff_size, Opt_err
};

static const match_table_t tokens = {
        { Opt_addr,          "physaddr=%x"        },
        { Opt_bpi,           "bpi=%u"             },
        { Opt_size,          "size=%s"            },
        { Opt_jsize,     "jsize=%s"               },
        { Opt_num_inodes,"num_inodes=%u"  },
        { Opt_mode,          "mode=%o"            },
        { Opt_uid,           "uid=%u"             },
        { Opt_gid,           "gid=%u"             },
        { Opt_wprotect,      "wprotect"           },
        { Opt_wprotectold,   "wprotectold"        },
        { Opt_err_cont,      "errors=continue"    },
        { Opt_err_panic,     "errors=panic"       },
        { Opt_err_ro,        "errors=remount-ro"  },
        { Opt_backing,       "backing=%s"         },
        { Opt_backing_opt,   "backing_opt=%u"     },
        { Opt_hugemmap,      "hugemmap"           },
        { Opt_nohugeioremap, "nohugeioremap"      },
        { Opt_dbgmask,       "dbgmask=%u"         },
        { Opt_vmmode,  	     "vmmode=%u"  	  },
	{ Opt_dentry_size,    "dsize=%s"	  },
	{ Opt_inodeoff_size,  "inodeoffsz=%s"     },
        { Opt_err,           NULL                 },
};



int crfss_parse_options2(char *options, struct crfss_sb_info *sbi,
			       bool remount)
{
	char *p, *rest;
	substring_t args[MAX_OPT_ARGS];
	int option;

	if (!options)
		return 0;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {

		case Opt_vmmode:
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->vmmode = option;
			break;
		case Opt_addr:
			if (remount)
				goto bad_opt;
			/* physaddr managed in get_phys_addr() */
			break;

		case Opt_dbgmask:
			if (match_int(&args[0], &option))
				goto bad_val;
			crfss_dbgmask = option;
			printk(KERN_ALERT "crfss_dbgmask=%d set? \n",crfss_dbgmask);
			break;

		case Opt_size:
			if (remount)
				goto bad_opt;
			/* memparse() will accept a K/M/G without a digit */
			if (!isdigit(*args[0].from))
				goto bad_val;
			sbi->initsize = memparse(args[0].from, &rest);
			printk(KERN_ALERT "initsize=%lu set? \n",sbi->initsize);
			/*set_opt(sbi->s_mount_opt, FORMAT);*/
			break;

		case Opt_dentry_size:
			if (remount)
				goto bad_opt;
			/* memparse() will accept a K/M/G without a digit */
			if (!isdigit(*args[0].from))
				goto bad_val;

#if defined(_DEVFS_DENTRY_OFFLOAD)
			sbi->dentrysize = memparse(args[0].from, &rest);
#endif
			printk(KERN_ALERT "dentrysize=%lu set? \n",sbi->dentrysize);
			/*set_opt(sbi->s_mount_opt, FORMAT);*/
			break;

		case Opt_inodeoff_size:
			if (remount)
				goto bad_opt;
			/* memparse() will accept a K/M/G without a digit */
			if (!isdigit(*args[0].from))
				goto bad_val;

#if defined(_DEVFS_INODE_OFFLOAD)
			sbi->inodeoffsz = memparse(args[0].from, &rest);
#else
			sbi->inodeoffsz = 0;
#endif
			printk(KERN_ALERT "inodeoffsz=%lu set? \n",sbi->inodeoffsz);
			break;
#if 0
		case Opt_bpi:
			if (remount)
				goto bad_opt;
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->bpi = option;
			break;
		case Opt_uid:
			if (remount)
				goto bad_opt;
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->uid = make_kuid(current_user_ns(), option);
			break;
		case Opt_gid:
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->gid = make_kgid(current_user_ns(), option);
			break;
		case Opt_mode:
			if (match_octal(&args[0], &option))
				goto bad_val;
			sbi->mode = option & 01777U;
			break;
		case Opt_jsize:
			if (remount)
				goto bad_opt;
			/* memparse() will accept a K/M/G without a digit */
			if (!isdigit(*args[0].from))
				goto bad_val;
			sbi->jsize = memparse(args[0].from, &rest);
			/* make sure journal size is integer power of 2 */
			if (sbi->jsize & (sbi->jsize - 1) ||
				sbi->jsize < PMFS_MINIMUM_JOURNAL_SIZE) {
				crfs_dbg("Invalid jsize: "
					"must be whole power of 2 & >= 64KB\n");
				goto bad_val;
			}
			break;
		case Opt_num_inodes:
			if (remount)
				goto bad_opt;
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->num_inodes = option;
			break;
		case Opt_err_panic:
			clear_opt(sbi->s_mount_opt, ERRORS_CONT);
			clear_opt(sbi->s_mount_opt, ERRORS_RO);
			set_opt(sbi->s_mount_opt, ERRORS_PANIC);
			break;
		case Opt_err_ro:
			clear_opt(sbi->s_mount_opt, ERRORS_CONT);
			clear_opt(sbi->s_mount_opt, ERRORS_PANIC);
			set_opt(sbi->s_mount_opt, ERRORS_RO);
			break;
		case Opt_err_cont:
			clear_opt(sbi->s_mount_opt, ERRORS_RO);
			clear_opt(sbi->s_mount_opt, ERRORS_PANIC);
			set_opt(sbi->s_mount_opt, ERRORS_CONT);
			break;
		case Opt_wprotect:
			if (remount)
				goto bad_opt;
			set_opt(sbi->s_mount_opt, PROTECT);
			crfs_info
				("PMFS: Enabling new Write Protection (CR0.WP)\n");
			break;
		case Opt_wprotectold:
			if (remount)
				goto bad_opt;
			set_opt(sbi->s_mount_opt, PROTECT_OLD);
			crfs_info
				("PMFS: Enabling old Write Protection (PAGE RW Bit)\n");
			break;
		case Opt_hugemmap:
			if (remount)
				goto bad_opt;
			set_opt(sbi->s_mount_opt, HUGEMMAP);
			crfs_info("PMFS: Enabling huge mappings for mmap\n");
			break;
		case Opt_nohugeioremap:
			if (remount)
				goto bad_opt;
			clear_opt(sbi->s_mount_opt, HUGEIOREMAP);
			crfs_info("PMFS: Disabling huge ioremap\n");
			break;
			break;
		case Opt_backing:
			strncpy(sbi->crfs_backing_file, args[0].from, 255);
			break;
		case Opt_backing_opt:
			if (match_int(&args[0], &option))
				goto bad_val;
			sbi->crfs_backing_option = option;
			break;
#endif
		default: {
			goto bad_opt;
		}
		}
	}

	return 0;

bad_val:
	printk(KERN_INFO "Bad value '%s' for mount option '%s'\n", args[0].from,
	       p);
	return -EINVAL;
bad_opt:
	printk(KERN_INFO "Bad mount option: \"%s\"\n", p);
	return -EINVAL;
}

#endif

phys_addr_t get_phys_addr(void **data)
{
        phys_addr_t phys_addr;
        char *options = (char *)*data;

        if (!options || strncmp(options, "physaddr=", 9) != 0)
                return (phys_addr_t)ULLONG_MAX;
        options += 9;
        phys_addr = (phys_addr_t)simple_strtoull(options, &options, 0);
        if (*options && *options != ',') {
                printk(KERN_ERR "Invalid phys addr specification: %s\n",
                       (char *)*data);
                return (phys_addr_t)ULLONG_MAX;
        }
        if (phys_addr & (PAGE_SIZE - 1)) {
                printk(KERN_ERR "physical address 0x%16llx for devfs isn't "
                       "aligned to a page boundary\n", (u64)phys_addr);
                return (phys_addr_t)ULLONG_MAX;
        }
        if (*options == ',')
                options++;
        *data = (void *)options;
        return phys_addr;
}



static int __init init_rangenode_cache(void)
{
	crfss_range_node_cachep = kmem_cache_create("crfss_range_node_cache",
					sizeof(struct crfss_range_node),
					0, (SLAB_RECLAIM_ACCOUNT |
                                        SLAB_MEM_SPREAD), NULL);
	if (crfss_range_node_cachep == NULL)
		return -ENOMEM;
	return 0;
}


/*Set the devfs block size*/
static void crfss_set_blocksize(struct super_block *sb, unsigned long size)
{
	int bits;

	bits = fls(size) - 1;
	sb->s_blocksize_bits = bits;
	sb->s_blocksize = (1 << bits);
}

inline void crfss_set_default_opts(struct crfss_sb_info *sbi)
{
        //set_opt(sbi->s_mount_opt, HUGEIOREMAP);
        //set_opt(sbi->s_mount_opt, ERRORS_CONT);
        sbi->reserved_blocks = RESERVED_BLOCKS;
        sbi->cpus = num_online_cpus();
        sbi->map_id = 0;
}

struct devfss_inode *devfss_init(struct super_block *sb,
				      unsigned long size)
{
	unsigned long blocksize;
	unsigned long reserved_space, reserved_blocks;
	struct devfss_inode *root_i, *pi;
	struct crfss_super_block *super;
	struct crfss_sb_info *sbi = DEVFS_SB(sb);


	sbi->num_blocks = ((unsigned long)(size) >> PAGE_SHIFT);
	sbi->cpus = 4; //num_online_cpus();

	if(init_rangenode_cache()){
		printk("%s:%d init_rangenode_cache \n",__FUNCTION__,__LINE__);
                return ERR_PTR(-EINVAL);
	}

	if(!sbi->phys_addr) {
		printk("FAILED %s:%d sbi->phys_addr %llu \n",
			__FUNCTION__,__LINE__, sbi->phys_addr);
		goto alloc_virtaddr;
	}
	
	if(sbi->vmmode) {
		sbi->virt_addr = NULL;
		sbi->d_host_addr = NULL;
		sbi->i_host_addr = NULL;
		sbi->d_host_off = 0;
		sbi->i_host_addr = 0;
		sbi->i_host_off = 0;
	}else {
		//Just use one ioremap
		sbi->initsize = sbi->initsize + sbi->dentrysize + sbi->inodeoffsz;

		printk("creating an empty devfs of size %lu\n", sbi->initsize);

		sbi->virt_addr = crfss_ioremap(sb, sbi->phys_addr, sbi->initsize, "devfsmeta");
		if(!sbi->virt_addr) {
			printk("FAILED %s:%d sbi->phys_addr %llu sbi->initsize %lu \n",
				__FUNCTION__,__LINE__, sbi->phys_addr, sbi->initsize);
			return NULL;
		}

		//Resize file system size by excluding the offload portion
		sbi->initsize = sbi->initsize - sbi->dentrysize - sbi->inodeoffsz;


#if defined(_DEVFS_DENTRY_OFFLOAD)
		sbi->d_host_addr = sbi->virt_addr + sbi->initsize;
#else
		sbi->d_host_addr = 0;
#endif
		//Current offset of dentry host location
		sbi->d_host_off = 0;

#if defined(_DEVFS_INODE_OFFLOAD)
		sbi->i_host_addr = sbi->virt_addr + sbi->initsize + sbi->dentrysize;
#else
		sbi->i_host_addr = 0;
#endif
	}


#if defined(_DEVFS_PMFS_BALLOC)
	sbi->block_start = (unsigned long)0;
	sbi->block_end = ((unsigned long)(sbi->initsize) >> PAGE_SHIFT);
	sbi->num_free_blocks = ((unsigned long)(sbi->initsize) >> PAGE_SHIFT);
	crfs_init_blockmap(sb, 0);
	//TODO:
	//crfs_memlock_range(sb, super, journal_data_start);
#endif

	crfss_dbgv("creating an empty devfs of size %lu\n", sbi->initsize);
	
alloc_virtaddr:
	//If ioremap failed, which will in VMs, allocated using kzalloc
	//TO Be removed 
	if (!sbi->virt_addr) {
		//printk(KERN_ERR "ioremap of the devfs image failed(1)\n");
		//return ERR_PTR(-EINVAL);
		sbi->virt_addr = kzalloc(size, GFP_KERNEL);
		if (!sbi->virt_addr) {
			printk(KERN_ERR "sbi->virt_addr allocation failed\n");
			return ERR_PTR(-EINVAL);
		}

	}

	crfss_dbgv("Before _DEVFS_DENTRY_OFFLOAD %lu\n", sbi->virt_addr);

#if defined(_DEVFS_DENTRY_OFFLOAD)
	//If ioremap failed, which will in VMs, allocated using kzalloc
	//TO Be removed 
	if (!sbi->d_host_addr) {
		sbi->dentrysize = 2097152;
		sbi->d_host_addr = kzalloc(sbi->dentrysize, GFP_KERNEL);
		if (!sbi->d_host_addr) {
			printk(KERN_ERR "sbi->d_host_addr allocation failed\n");
			return ERR_PTR(-EINVAL);
		}

	}
#endif
	crfss_dbgv("Before _DEVFS_DENTRY_OFFLOAD %lu\n", sbi->d_host_addr);

#if defined(_DEVFS_INODE_OFFLOAD)
	//If ioremap failed, which will in VMs, allocated using kzalloc
	//TO Be removed 
	if (!sbi->i_host_addr) {
		sbi->inodeoffsz = 2097152;
		sbi->i_host_addr = kzalloc(sbi->inodeoffsz, GFP_KERNEL);
		if (!sbi->i_host_addr) {
			printk(KERN_ERR "sbi->i_host_addr allocation failed\n");
			return ERR_PTR(-EINVAL);
		}

	}
#endif
	printk(KERN_ALERT "ioremap size %lu sbi->phys_addr %llu "
		"sbi->d_host_addr %lu sbi->dentrysize %lu  "
		"sbi->i_host_addr %lu sbi->inodeoffsz %lu \n", 
		sbi->initsize, sbi->phys_addr, sbi->d_host_addr, 
		sbi->dentrysize, sbi->i_host_addr, sbi->inodeoffsz);

	blocksize = sbi->blocksize = DEVFS_DEF_BLOCK_SIZE_4K;
	crfss_set_blocksize(sb, blocksize);
	blocksize = sb->s_blocksize;

	if (sbi->blocksize && sbi->blocksize != blocksize)
		sbi->blocksize = blocksize;

	/*if (!crfss_check_size(sb, size)) {
		crfss_dbg("Specified NOVA size too small 0x%lx.\n", size);
		return ERR_PTR(-EINVAL);
	}*/

	/* Reserve space for 8 special inodes */
	reserved_space = DEVFS_SB_SIZE * 4;
	reserved_blocks = (reserved_space + blocksize - 1) / blocksize;
	if (reserved_blocks > sbi->reserved_blocks) {
		printk("Reserved %lu blocks, require %lu blocks. "
			"Increase reserved blocks number.\n",
			sbi->reserved_blocks, reserved_blocks);
		return ERR_PTR(-EINVAL);
	}

	printk("max file name len %d\n", (unsigned int)DEVFS_NAME_LEN);

	super = crfss_get_super(sb);

	/* clear out super-block and inode table */
	crfss_memset_nt(super, 0, sbi->reserved_blocks * sbi->blocksize);
	super->s_size = cpu_to_le64(size);
	super->s_blocksize = cpu_to_le32(blocksize);
	super->s_magic = cpu_to_le32(DEVFS_SUPER_MAGIC);

	if(crfss_init_blockmap(sb, 0)) {
		printk("%s:%d Failed crfss_init_blockmap \n",
			__FUNCTION__,__LINE__);
	}

#if defined(_DEVFS_SLAB_ALLOC)
	if(sbi->i_host_addr) {
		sbi->i_host_slab = crfss_slab_init(sbi, sizeof(struct inode));
		BUG_ON(!sbi->i_host_slab);
	}
#endif

//#if  defined(_DEVFS_DENTRY_OFFLOAD) && defined(_DEVFS_SLAB_ALLOC)
#if defined(_DEVFS_SLAB_ALLOC)
	if(sbi->d_host_addr) {
		sbi->d_host_slab = crfss_slab_init(sbi, sizeof(struct dentry));
		BUG_ON(!sbi->d_host_slab);
	}
#endif


#if 0
	if (crfss_lite_journal_hard_init(sb) < 0) {
		printk(KERN_ERR "Lite journal hard initialization failed\n");
		return ERR_PTR(-EINVAL);
	}
#endif
	if (crfss_init_inode_inuse_list(sb) < 0)
		return ERR_PTR(-EINVAL);

	if (crfss_init_inode_table(sb) < 0) {
		printk("%s:%d Failed crfss_init_inode_table\n",
			__FUNCTION__,__LINE__);
		return ERR_PTR(-EINVAL);
	}

#if 0

	pi = (struct devfss_inode *)crfss_get_inode_by_ino
			(sb, DEVFS_BLOCKNODE_INO);

	pi->nova_ino = DEVFS_BLOCKNODE_INO;

	crfss_dbgv("%s:%d Afrer crfss_get_inode_by_ino\n",
			__FUNCTION__,__LINE__);


	pi = (struct devfss_inode *)crfss_get_inode_by_ino
			(sb, DEVFS_INODELIST_INO);
	pi->nova_ino = DEVFS_INODELIST_INO;
#endif


	crfss_dbgv("%s:%d Afrer crfss_get_inode_by_ino\n",
			__FUNCTION__,__LINE__);

	root_i = crfss_get_inode_by_ino(sb, DEVFS_ROOT_INO);

#if defined(_DEVFS_NOVA_LOG)

	if(!root_i) {
		printk("%s:%d root_i NULL \n",
			__FUNCTION__,__LINE__);
	}

	if(crfss_append_dir_init_entries(sb, root_i, 
			DEVFS_ROOT_INO, DEVFS_ROOT_INO)) {
		printk("%s:%d dir_init_entries failed\n",
			 __FUNCTION__,__LINE__);	
	}
#endif

#if 0 //_DEVFS_NOVA_YETTO
	nova_flush_buffer(pi, CACHELINE_SIZE, 1);

	nova_memunlock_range(sb, super, DEVFS_SB_SIZE*2);
	nova_sync_super(super);
	nova_memlock_range(sb, super, DEVFS_SB_SIZE*2);

	nova_flush_buffer(super, DEVFS_SB_SIZE, false);
	nova_flush_buffer((char *)super + DEVFS_SB_SIZE, sizeof(*super), false);

	//nova_dbg_verbose("Allocate root inode\n");

	root_i = crfss_get_inode_by_ino(sb, DEVFS_ROOT_INO);

	nova_memunlock_inode(sb, root_i);

	root_i->i_mode = cpu_to_le16(sbi->mode | S_IFDIR);
	root_i->i_uid = cpu_to_le32(from_kuid(&init_user_ns, sbi->uid));
	root_i->i_gid = cpu_to_le32(from_kgid(&init_user_ns, sbi->gid));
	root_i->i_links_count = cpu_to_le16(2);
	root_i->i_blk_type = DEVFS_BLOCK_TYPE_4K;
	root_i->i_flags = 0;
	root_i->i_blocks = cpu_to_le64(1);
	root_i->i_size = cpu_to_le64(sb->s_blocksize);
	root_i->i_atime = root_i->i_mtime = root_i->i_ctime =
		cpu_to_le32(get_seconds());
	root_i->nova_ino = DEVFS_ROOT_INO;
	root_i->valid = 1;
	/* nova_sync_inode(root_i); */
	nova_memlock_inode(sb, root_i);
	nova_flush_buffer(root_i, sizeof(*root_i), false);

	nova_append_dir_init_entries(sb, root_i, DEVFS_ROOT_INO,
					DEVFS_ROOT_INO);

	PERSISTENT_MARK();
	PERSISTENT_BARRIER();

	//DEVFS_END_TIMING(new_init_t, init_time);
#endif //_DEVFS_NOVA_YETTO
	return root_i;
}

#if 0
struct devfss_inode *devfss_init(struct super_block *sb,
				      unsigned long size)
{
	unsigned long blocksize;
	unsigned long reserved_space, reserved_blocks;
	struct devfss_inode *root_i, *pi;
	struct crfss_super_block *super;
	struct crfss_sb_info *sbi = DEVFS_SB(sb);


	printk("creating an empty devfs of size %lu\n", size);
	sbi->num_blocks = ((unsigned long)(size) >> PAGE_SHIFT);

	if(init_rangenode_cache()){
		printk("%s:%d init_rangenode_cache \n",__FUNCTION__,__LINE__);
                return ERR_PTR(-EINVAL);
	}


	if(!sbi->phys_addr) {
		printk("FAILED %s:%d sbi->phys_addr %lu \n",
			__FUNCTION__,__LINE__, sbi->phys_addr);
		goto alloc_virtaddr;
	}
	
	printk(KERN_ALERT "trying to execute ioremap size %lu "
		"sbi->phys_addr %lu \n", sbi->phys_addr, size);

#if defined(_DEVFS_VM)
	 sbi->virt_addr = NULL;
#else
	sbi->virt_addr = crfss_ioremap(sb, sbi->phys_addr, size);
#endif

	printk(KERN_ALERT "After executing ioremap size %lu "
		"sbi->virt_addr %lu \n", sbi->virt_addr, size);

alloc_virtaddr:

	if (!sbi->virt_addr) {
		//printk(KERN_ERR "ioremap of the devfs image failed(1)\n");
		//return ERR_PTR(-EINVAL);
		sbi->virt_addr = kzalloc(size, GFP_KERNEL);
		if (!sbi->virt_addr) {
			printk(KERN_ERR "ioremap of the devfs image failed(1)\n");
			return ERR_PTR(-EINVAL);
		}

	}

	printk("devfs: Default block size set to 4K\n");

	blocksize = sbi->blocksize = DEVFS_DEF_BLOCK_SIZE_4K;

	crfss_set_blocksize(sb, blocksize);
	blocksize = sb->s_blocksize;

	if (sbi->blocksize && sbi->blocksize != blocksize)
		sbi->blocksize = blocksize;

	/*if (!crfss_check_size(sb, size)) {
		crfss_dbg("Specified NOVA size too small 0x%lx.\n", size);
		return ERR_PTR(-EINVAL);
	}*/

	/* Reserve space for 8 special inodes */
	reserved_space = DEVFS_SB_SIZE * 4;
	reserved_blocks = (reserved_space + blocksize - 1) / blocksize;
	if (reserved_blocks > sbi->reserved_blocks) {
		printk("Reserved %lu blocks, require %lu blocks. "
			"Increase reserved blocks number.\n",
			sbi->reserved_blocks, reserved_blocks);
		return ERR_PTR(-EINVAL);
	}

	printk("max file name len %d\n", (unsigned int)DEVFS_NAME_LEN);

	super = crfss_get_super(sb);

	/* clear out super-block and inode table */
	memset_nt(super, 0, sbi->reserved_blocks * sbi->blocksize);
	super->s_size = cpu_to_le64(size);
	super->s_blocksize = cpu_to_le32(blocksize);
	super->s_magic = cpu_to_le32(DEVFS_SUPER_MAGIC);

	if(crfss_init_blockmap(sb, 0)) {
		printk("%s:%d Failed crfss_init_blockmap \n",
			__FUNCTION__,__LINE__);
	}

#if 0
	if (crfss_lite_journal_hard_init(sb) < 0) {
		printk(KERN_ERR "Lite journal hard initialization failed\n");
		return ERR_PTR(-EINVAL);
	}
#endif
	if (crfss_init_inode_inuse_list(sb) < 0)
		return ERR_PTR(-EINVAL);

	if (crfss_init_inode_table(sb) < 0) {
		printk("%s:%d Failed crfss_init_inode_table\n",
			__FUNCTION__,__LINE__);
		return ERR_PTR(-EINVAL);
	}

	pi = (struct devfss_inode *)crfss_get_inode_by_ino
			(sb, DEVFS_BLOCKNODE_INO);

	pi->nova_ino = DEVFS_BLOCKNODE_INO;

	printk("%s:%d Afrer crfss_get_inode_by_ino\n",
			__FUNCTION__,__LINE__);


#if 0 //_DEVFS_NOVA_YETTO
	nova_flush_buffer(pi, CACHELINE_SIZE, 1);
#endif
	pi = (struct devfss_inode *)crfss_get_inode_by_ino
			(sb, DEVFS_INODELIST_INO);
	pi->nova_ino = DEVFS_INODELIST_INO;

	printk("%s:%d Afrer crfss_get_inode_by_ino\n",
			__FUNCTION__,__LINE__);

	root_i = crfss_get_inode_by_ino(sb, DEVFS_ROOT_INO);

#if defined(_DEVFS_NOVA_LOG)
	if(crfss_append_dir_init_entries(sb, root_i, 
			DEVFS_ROOT_INO, DEVFS_ROOT_INO)) {
		printk("%s:%d dir_init_entries failed\n",
			 __FUNCTION__,__LINE__);	
	}
#endif
	root_i->i_blk_type = DEVFS_BLOCK_TYPE_2M;

#if 0 //_DEVFS_NOVA_YETTO
	nova_flush_buffer(pi, CACHELINE_SIZE, 1);

	nova_memunlock_range(sb, super, DEVFS_SB_SIZE*2);
	nova_sync_super(super);
	nova_memlock_range(sb, super, DEVFS_SB_SIZE*2);

	nova_flush_buffer(super, DEVFS_SB_SIZE, false);
	nova_flush_buffer((char *)super + DEVFS_SB_SIZE, sizeof(*super), false);

	//nova_dbg_verbose("Allocate root inode\n");

	root_i = crfss_get_inode_by_ino(sb, DEVFS_ROOT_INO);

	nova_memunlock_inode(sb, root_i);

	root_i->i_mode = cpu_to_le16(sbi->mode | S_IFDIR);
	root_i->i_uid = cpu_to_le32(from_kuid(&init_user_ns, sbi->uid));
	root_i->i_gid = cpu_to_le32(from_kgid(&init_user_ns, sbi->gid));
	root_i->i_links_count = cpu_to_le16(2);
	root_i->i_blk_type = DEVFS_BLOCK_TYPE_4K;
	root_i->i_flags = 0;
	root_i->i_blocks = cpu_to_le64(1);
	root_i->i_size = cpu_to_le64(sb->s_blocksize);
	root_i->i_atime = root_i->i_mtime = root_i->i_ctime =
		cpu_to_le32(get_seconds());
	root_i->nova_ino = DEVFS_ROOT_INO;
	root_i->valid = 1;
	/* nova_sync_inode(root_i); */
	nova_memlock_inode(sb, root_i);
	nova_flush_buffer(root_i, sizeof(*root_i), false);

	nova_append_dir_init_entries(sb, root_i, DEVFS_ROOT_INO,
					DEVFS_ROOT_INO);

	PERSISTENT_MARK();
	PERSISTENT_BARRIER();

	//DEVFS_END_TIMING(new_init_t, init_time);
#endif //_DEVFS_NOVA_YETTO
	return root_i;
}

#endif










inline void crfss_free_range_node(struct crfss_range_node *node)
{
	kmem_cache_free(crfss_range_node_cachep, node);
}

inline void crfss_free_blocknode(struct super_block *sb,
	struct crfss_range_node *node)
{
	crfss_free_range_node(node);
}

inline void crfss_free_inode_node(struct super_block *sb,
	struct crfss_range_node *node)
{
	crfss_free_range_node(node);
}

static inline
struct crfss_range_node *crfss_alloc_range_node(struct super_block *sb)
{
	struct crfss_range_node *p;
	p = (struct crfss_range_node *)
		kmem_cache_alloc(crfss_range_node_cachep, GFP_NOFS);
	return p;
}

inline struct crfss_range_node *crfss_alloc_blocknode(struct super_block *sb)
{
	return crfss_alloc_range_node(sb);
}

inline struct crfss_range_node *crfss_alloc_inode_node(struct super_block *sb)
{
	return crfss_alloc_range_node(sb);
}

#if 0
static struct inode *crfss_alloc_inode(struct super_block *sb)
{
	struct crfss_inode_info *vi;

	vi = kmem_cache_alloc(crfss_inode_cachep, GFP_NOFS);
	if (!vi)
		return NULL;

	vi->vfs_inode.i_version = 1;

	return &vi->vfs_inode;
}

static void crfss_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	struct crfss_inode_info *vi = NOVA_I(inode);

	printk("%s: ino %lu\n", __func__, inode->i_ino);
	kmem_cache_free(crfss_inode_cachep, vi);
}

static void crfss_destroy_inode(struct inode *inode)
{
	printk("%s: %lu\n", __func__, inode->i_ino);
	call_rcu(&inode->i_rcu, crfss_i_callback);
}

static void init_once(void *foo)
{
	struct crfss_inode_info *vi = foo;

	inode_init_once(&vi->vfs_inode);
}
#endif
#endif








