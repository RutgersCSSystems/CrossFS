/*
 * BRIEF DESCRIPTION
 *
 * File operations for directories.
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
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
#include <linux/devfs.h>

#define DT2IF(dt) (((dt) << 12) & S_IFMT)
#define IF2DT(sif) (((sif) & S_IFMT) >> 12)

struct crfss_dentry *crfss_find_dentry(struct super_block *sb,
	struct devfss_inode *pi, struct inode *inode, const char *name,
	unsigned long name_len)
{
	struct crfss_inode *si = DEVFS_I(inode);
	struct crfss_dentry *direntry;
	unsigned long hash;

	hash = BKDRHash(name, name_len);
	direntry = radix_tree_lookup(&si->dentry_tree, hash);

	return direntry;
}

static int crfss_insert_dir_radix_tree(struct super_block *sb,
	struct crfss_inode *si, const char *name,
	int namelen, struct crfss_dentry *direntry)
{
	unsigned long hash;
	int ret;

	hash = BKDRHash(name, namelen);
	crfss_dbgv("%s: insert %s hash %lu\n", __func__, name, hash);

	/* FIXME: hash collision ignored here */
	ret = radix_tree_insert(&si->dentry_tree, hash, direntry);
	if (ret)
		printk("%s ERROR %d: %s\n", __func__, ret, name);

	return ret;
}

static int crfss_check_dentry_match(struct super_block *sb,
	struct crfss_dentry *dentry, const char *name, int namelen)
{
	if (dentry->name_len != namelen)
		return -EINVAL;

	return strncmp(dentry->name, name, namelen);
}

static int crfss_remove_dir_radix_tree(struct super_block *sb,
	struct crfss_inode *si, const char *name, int namelen,
	int replay)
{
	struct crfss_dentry *entry;
	unsigned long hash;

	hash = BKDRHash(name, namelen);
	entry = radix_tree_delete(&si->dentry_tree, hash);

	if (replay == 0) {
		if (!entry) {
			printk("%s ERROR: %s, length %d, hash %lu\n",
					__func__, name, namelen, hash);
			return -EINVAL;
		}

		//if (entry->ino == 0 || 
		if (entry->invalid ||
		    crfss_check_dentry_match(sb, entry, name, namelen)) {
			printk("%s dentry not match: %s, length %d, "
					"hash %lu\n", __func__, name,
					namelen, hash);
			printk("dentry: type %d, inode %llu, name %s, "
					"namelen %u, rec len %u\n",
					entry->entry_type,
					le64_to_cpu(entry->ino),
					entry->name, entry->name_len,
					le16_to_cpu(entry->de_len));
			return -EINVAL;
		}

		/* No need to flush */
		entry->invalid = 1;
	}

	return 0;
}

void crfss_delete_dir_tree(struct super_block *sb,
	struct crfss_inode *si)
{
	struct crfss_dentry *direntry;
	unsigned long pos = 0;
	struct crfss_dentry *entries[FREE_BATCH];
	int nr_entries;
	int i;
	void *ret;

	//DEVFS_START_TIMING(delete_dir_tree_t, delete_time);

	do {
		nr_entries = radix_tree_gang_lookup(&si->dentry_tree,
					(void **)entries, pos, FREE_BATCH);
		for (i = 0; i < nr_entries; i++) {
			direntry = entries[i];
			BUG_ON(!direntry);
			pos = BKDRHash(direntry->name, direntry->name_len);
			ret = radix_tree_delete(&si->dentry_tree, pos);
			if (!ret || ret != direntry) {
				printk( "dentry: type %d, inode %llu, "
					"name %s, namelen %u, rec len %u\n",
					direntry->entry_type,
					le64_to_cpu(direntry->ino),
					direntry->name, direntry->name_len,
					le16_to_cpu(direntry->de_len));
				if (!ret)
					printk("ret is NULL\n");
			}
		}
		pos++;
	} while (nr_entries == FREE_BATCH);

	//NOVA_END_TIMING(delete_dir_tree_t, delete_time);
	return;
}

/* ========================= Entry operations ============================= */

/*
 * Append a crfss_dentry to the current devfss_inode_log_page.
 * Note unlike append_file_write_entry(), this method returns the tail pointer
 * after append.
 */
static u64 crfss_append_dir_inode_entry(struct super_block *sb,
	struct devfss_inode *pidir, struct inode *dir,
	u64 ino, struct dentry *dentry, unsigned short de_len, u64 tail,
	int link_change, u64 *curr_tail)
{
	struct crfss_inode *si = DEVFS_I(dir);
	struct crfss_dentry *entry;
	u64 curr_p;
	size_t size = de_len;
	int extended = 0;
	unsigned short links_count;

	//NOVA_START_TIMING(append_dir_entry_t, append_time);

	curr_p = crfss_get_append_head(sb, pidir, si, tail, size, &extended);
	if (curr_p == 0)
		BUG();

	entry = (struct crfss_dentry *)crfss_get_block(sb, curr_p);
	entry->entry_type = DIR_LOG;
	entry->ino = cpu_to_le64(ino);
	entry->name_len = dentry->d_name.len;
#if defined(YET_TOBE_DEFINED)
	memcpy_to_pmem_nocache(entry->name, dentry->d_name.name,
				dentry->d_name.len);
#else
	memcpy(entry->name, dentry->d_name.name,
				dentry->d_name.len);
#endif

	entry->name[dentry->d_name.len] = '\0';
	entry->file_type = 0;
	entry->invalid = 0;
	entry->mtime = cpu_to_le32(dir->i_mtime.tv_sec);
	entry->size = cpu_to_le64(dir->i_size);

	links_count = cpu_to_le16(dir->i_nlink);
	if (links_count == 0 && link_change == -1)
		links_count = 0;
	else
		links_count += link_change;
	entry->links_count = cpu_to_le16(links_count);

	/* Update actual de_len */
	entry->de_len = cpu_to_le16(de_len);
	crfss_dbgv("dir entry @ 0x%llx: ino %llu, entry len %u, "
			"name len %u, file type %u\n",
			curr_p, entry->ino, entry->de_len,
			entry->name_len, entry->file_type);

	//nova_flush_buffer(entry, de_len, 0);

	*curr_tail = curr_p + de_len;

	dir->i_blocks = pidir->i_blocks;
	//NOVA_END_TIMING(append_dir_entry_t, append_time);
	return curr_p;
}

/* Append . and .. entries */
int crfss_append_dir_init_entries(struct super_block *sb,
	struct devfss_inode *pi, u64 self_ino, u64 parent_ino)
{
	int allocated;
	u64 new_block;
	u64 curr_p;
	struct crfss_dentry *de_entry;

	if (pi->log_head) {
		printk("%s: log head exists @ 0x%llx!\n",
				__func__, pi->log_head);
		return - EINVAL;
	}

	allocated = crfss_allocate_inode_log_pages(sb, pi, 1, &new_block);
	if (allocated != 1) {
		printk( "ERROR: no inode log page available\n");
		return - ENOMEM;
	}
	pi->log_tail = pi->log_head = new_block;
	pi->i_blocks = 1;
	//nova_flush_buffer(&pi->log_head, CACHELINE_SIZE, 0);

	de_entry = (struct crfss_dentry *)crfss_get_block(sb, new_block);
	de_entry->entry_type = DIR_LOG;
	de_entry->ino = cpu_to_le64(self_ino);
	de_entry->name_len = 1;
	de_entry->de_len = cpu_to_le16(DEVFS_DIR_LOG_REC_LEN(1));
	de_entry->mtime = CURRENT_TIME_SEC.tv_sec;
	de_entry->size = sb->s_blocksize;
	de_entry->links_count = 1;
	strncpy(de_entry->name, ".\0", 2);
	//nova_flush_buffer(de_entry, DEVFS_DIR_LOG_REC_LEN(1), 0);

	curr_p = new_block + DEVFS_DIR_LOG_REC_LEN(1);

	de_entry = (struct crfss_dentry *)((char *)de_entry +
					le16_to_cpu(de_entry->de_len));
	de_entry->entry_type = DIR_LOG;
	de_entry->ino = cpu_to_le64(parent_ino);
	de_entry->name_len = 2;
	de_entry->de_len = cpu_to_le16(DEVFS_DIR_LOG_REC_LEN(2));
	de_entry->mtime = CURRENT_TIME_SEC.tv_sec;
	de_entry->size = sb->s_blocksize;
	de_entry->links_count = 2;
	strncpy(de_entry->name, "..\0", 3);
	//nova_flush_buffer(de_entry, DEVFS_DIR_LOG_REC_LEN(2), 0);

	curr_p += DEVFS_DIR_LOG_REC_LEN(2);
	crfss_update_tail(pi, curr_p);

	crfss_dbgv("%s: crfss_update_tail %llu\n",
			__func__, pi->log_tail);
	return 0;
}

/* adds a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int crfss_add_dentry(struct dentry *dentry, u64 ino, int inc_link,
	u64 tail, u64 *new_tail)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block *sb = dir->i_sb;
	struct crfss_inode *si = DEVFS_I(dir);
	struct devfss_inode *pidir;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct crfss_dentry *direntry;
	unsigned short loglen;
	int ret;
	u64 curr_entry, curr_tail;

	crfss_dbgv("%s: dir %lu new inode %llu\n",
				__func__, dir->i_ino, ino);
	crfss_dbgv("%s: %s %d\n", __func__, name, namelen);
	//NOVA_START_TIMING(add_dentry_t, add_dentry_time);
	if (namelen == 0)
		return -EINVAL;

	pidir = devfss_get_inode(sb, dir);

	//crfss_dbgv("%s: %s %lu tail %llu, head %llu\n", __func__, 
	//		name, pidir->nova_ino, pidir->log_tail, pidir->log_head);

	crfss_dbgv("%s: %s %lu \n", __func__, name, pidir->nova_ino);


	/*
	 * XXX shouldn't update any times until successful
	 * completion of syscall, but too many callers depend
	 * on this.
	 */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;

	loglen = DEVFS_DIR_LOG_REC_LEN(namelen);
	curr_entry = crfss_append_dir_inode_entry(sb, pidir, dir, ino,
				dentry,	loglen, tail, inc_link,
				&curr_tail);

	direntry = (struct crfss_dentry *)crfss_get_block(sb, curr_entry);
	ret = crfss_insert_dir_radix_tree(sb, si, name, namelen, direntry);
	*new_tail = curr_tail;
	crfss_dbgv("%s: %s curr_tail %llu \n", __func__, name, curr_tail);
	//NOVA_END_TIMING(add_dentry_t, add_dentry_time);

	if(!crfss_find_dentry(sb, pidir, dir, name, namelen)){
		crfss_dbgv("%s: Failed to locate %s \n", __func__, name);
	}else {
		crfss_dbgv("%s: Found entry %s \n", __func__, name);
	}

	return ret;
}

/* removes a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int crfss_remove_dentry(struct dentry *dentry, int dec_link, u64 tail,
	u64 *new_tail)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block *sb = dir->i_sb;
	struct crfss_inode *si = DEVFS_I(dir);
	struct devfss_inode *pidir;
	struct qstr *entry = &dentry->d_name;
	unsigned short loglen;
	u64 curr_tail, curr_entry;

	//NOVA_START_TIMING(remove_dentry_t, remove_dentry_time);

	if (!dentry->d_name.len)
		return -EINVAL;

	pidir = devfss_get_inode(sb, dir);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;

	loglen = DEVFS_DIR_LOG_REC_LEN(entry->len);
	curr_entry = crfss_append_dir_inode_entry(sb, pidir, dir, 0,
				dentry, loglen, tail, dec_link, &curr_tail);
	*new_tail = curr_tail;

	crfss_remove_dir_radix_tree(sb, si, entry->name, entry->len, 0);
	//NOVA_END_TIMING(remove_dentry_t, remove_dentry_time);
	return 0;
}








#if 0

inline int nova_replay_add_dentry(struct super_block *sb,
	struct crfss_inode *si, struct crfss_dentry *entry)
{
	if (!entry->name_len)
		return -EINVAL;

	printk("%s: add %s\n", __func__, entry->name);
	return crfss_insert_dir_radix_tree(sb, si,
			entry->name, entry->name_len, entry);
}

inline int nova_replay_remove_dentry(struct super_block *sb,
	struct crfss_inode *si,
	struct crfss_dentry *entry)
{
	printk("%s: remove %s\n", __func__, entry->name);
	crfss_remove_dir_radix_tree(sb, si, entry->name,
					entry->name_len, 1);
	return 0;
}

static inline void nova_rebuild_dir_time_and_size(struct super_block *sb,
	struct devfss_inode *pi, struct crfss_dentry *entry)
{
	if (!entry || !pi)
		return;

	pi->i_ctime = entry->mtime;
	pi->i_mtime = entry->mtime;
	pi->i_size = entry->size;
	pi->i_links_count = entry->links_count;
}

int nova_rebuild_dir_inode_tree(struct super_block *sb,
	struct devfss_inode *pi, u64 pi_addr,
	struct crfss_inode *si)
{
	struct crfss_dentry *entry = NULL;
	struct nova_setattr_logentry *attr_entry = NULL;
	struct nova_link_change_entry *link_change_entry = NULL;
	struct devfss_inode_log_page *curr_page;
	u64 ino = pi->nova_ino;
	unsigned short de_len;
	void *addr;
	u64 curr_p;
	u64 next;
	u8 type;
	int ret;

	//DEVFS_START_TIMING(rebuild_dir_t, rebuild_time);
	printk("Rebuild dir %llu tree\n", ino);

	si->pi_addr = pi_addr;

	curr_p = pi->log_head;
	if (curr_p == 0) {
		printk( "Dir %llu log is NULL!\n", ino);
		BUG();
	}

	printk("Log head 0x%llx, tail 0x%llx\n",
				curr_p, pi->log_tail);

	si->log_pages = 1;
	while (curr_p != pi->log_tail) {
		if (goto_next_page(sb, curr_p)) {
			si->log_pages++;
			curr_p = next_log_page(sb, curr_p);
		}

		if (curr_p == 0) {
			printk( "Dir %llu log is NULL!\n", ino);
			BUG();
		}

		addr = (void *)crfss_get_block(sb, curr_p);
		type = nova_get_entry_type(addr);
		switch (type) {
			case SET_ATTR:
				attr_entry =
					(struct nova_setattr_logentry *)addr;
				nova_apply_setattr_entry(sb, pi, si,
								attr_entry);
				si->last_setattr = curr_p;
				curr_p += sizeof(struct nova_setattr_logentry);
				continue;
			case LINK_CHANGE:
				link_change_entry =
					(struct nova_link_change_entry *)addr;
				nova_apply_link_change_entry(pi,
							link_change_entry);
				si->last_link_change = curr_p;
				curr_p += sizeof(struct nova_link_change_entry);
				continue;
			case DIR_LOG:
				break;
			default:
				printk("%s: unknown type %d, 0x%llx\n",
							__func__, type, curr_p);
				NOVA_ASSERT(0);
		}

		entry = (struct crfss_dentry *)crfss_get_block(sb, curr_p);
		printk("curr_p: 0x%llx, type %d, ino %llu, "
			"name %s, namelen %u, rec len %u\n", curr_p,
			entry->entry_type, le64_to_cpu(entry->ino),
			entry->name, entry->name_len,
			le16_to_cpu(entry->de_len));

		if (entry->ino > 0) {
			if (entry->invalid == 0) {
				/* A valid entry to add */
				ret = nova_replay_add_dentry(sb, si, entry);
			}
		} else {
			/* Delete the entry */
			ret = nova_replay_remove_dentry(sb, si, entry);
		}

		if (ret) {
			printk( "%s ERROR %d\n", __func__, ret);
			break;
		}

		nova_rebuild_dir_time_and_size(sb, pi, entry);

		de_len = le16_to_cpu(entry->de_len);
		curr_p += de_len;
	}

	si->i_size = le64_to_cpu(pi->i_size);
	si->i_mode = le64_to_cpu(pi->i_mode);
	//nova_flush_buffer(pi, sizeof(struct devfss_inode), 0);

	/* Keep traversing until log ends */
	curr_p &= PAGE_MASK;
	curr_page = (struct devfss_inode_log_page *)crfss_get_block(sb, curr_p);
	while ((next = curr_page->page_tail.next_page) != 0) {
		si->log_pages++;
		curr_p = next;
		curr_page = (struct devfss_inode_log_page *)
			crfss_get_block(sb, curr_p);
	}

	pi->i_blocks = si->log_pages;

//	nova_print_dir_tree(sb, si, ino);
	//NOVA_END_TIMING(rebuild_dir_t, rebuild_time);
	return 0;
}
#endif

#if 0
static int nova_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	struct devfss_inode *pidir;
	struct crfss_inode *si = DEVFS_I(inode);
	struct crfss_inode *si = &si->header;
	struct devfss_inode *child_pi;
	struct crfss_dentry *entry;
	struct crfss_dentry *entries[FREE_BATCH];
	int nr_entries;
	u64 pi_addr;
	unsigned long pos = 0;
	ino_t ino;
	int i;
	int ret;

	//NOVA_START_TIMING(readdir_t, readdir_time);
	pidir = nova_get_inode(sb, inode);
	printk("%s: ino %llu, size %llu, pos %llu\n",
			__func__, (u64)inode->i_ino,
			pidir->i_size, ctx->pos);

	if (!si) {
		printk("%s: inode %lu si does not exist!\n",
				__func__, inode->i_ino);
		ctx->pos = READDIR_END;
		return 0;
	}

	pos = ctx->pos;
	if (pos == READDIR_END)
		goto out;

	do {
		nr_entries = radix_tree_gang_lookup(&si->dentry_tree,
					(void **)entries, pos, FREE_BATCH);
		for (i = 0; i < nr_entries; i++) {
			entry = entries[i];
			pos = BKDRHash(entry->name, entry->name_len);
			ino = __le64_to_cpu(entry->ino);
			if (ino == 0)
				continue;

			ret = crfss_get_inode_address(sb, ino, &pi_addr, 0);
			if (ret) {
				printk("%s: get child inode %lu address "
					"failed %d\n", __func__, ino, ret);
				ctx->pos = READDIR_END;
				return ret;
			}

			child_pi = crfss_get_block(sb, pi_addr);
			printk("ctx: ino %llu, name %s, "
				"name_len %u, de_len %u\n",
				(u64)ino, entry->name, entry->name_len,
				entry->de_len);
			if (!dir_emit(ctx, entry->name, entry->name_len,
				ino, IF2DT(le16_to_cpu(child_pi->i_mode)))) {
				printk("Here: pos %llu\n", ctx->pos);
				return 0;
			}
			ctx->pos = pos + 1;
		}
		pos++;
	} while (nr_entries == FREE_BATCH);

out:
	//NOVA_END_TIMING(readdir_t, readdir_time);
	return 0;
}
#endif

#if 0
static u64 nova_find_next_dentry_addr(struct super_block *sb,
	struct crfss_inode *si, u64 pos)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);
	struct nova_file_write_entry *entry = NULL;
	struct nova_file_write_entry *entries[1];
	int nr_entries;
	u64 addr = 0;

	nr_entries = radix_tree_gang_lookup(&si->dentry_tree,
					(void **)entries, pos, 1);
	if (nr_entries == 1) {
		entry = entries[0];
		addr = nova_get_addr_off(sbi, entry);
	}

	return addr;
}

static int nova_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	struct devfss_inode *pidir;
	struct crfss_inode *si = DEVFS_I(inode);
	struct crfss_inode *si = &si->header;
	struct devfss_inode *child_pi;
	struct devfss_inode *prev_child_pi = NULL;
	struct crfss_dentry *entry = NULL;
	struct crfss_dentry *prev_entry = NULL;
	unsigned short de_len;
	u64 pi_addr;
	unsigned long pos = 0;
	ino_t ino;
	void *addr;
	u64 curr_p;
	u8 type;
	int ret;

	//NOVA_START_TIMING(readdir_t, readdir_time);
	pidir = nova_get_inode(sb, inode);
	printk("%s: ino %llu, size %llu, pos 0x%llx\n",
			__func__, (u64)inode->i_ino,
			pidir->i_size, ctx->pos);

	if (pidir->log_head == 0) {
		printk( "Dir %lu log is NULL!\n", inode->i_ino);
		BUG();
		return -EINVAL;
	}

	pos = ctx->pos;

	if (pos == 0) {
		curr_p = pidir->log_head;
	} else if (pos == READDIR_END) {
		goto out;
	} else {
		curr_p = nova_find_next_dentry_addr(sb, si, pos);
		if (curr_p == 0)
			goto out;
	}

	while (curr_p != pidir->log_tail) {
		if (goto_next_page(sb, curr_p)) {
			curr_p = next_log_page(sb, curr_p);
		}

		if (curr_p == 0) {
			printk( "Dir %lu log is NULL!\n", inode->i_ino);
			BUG();
			return -EINVAL;
		}

		addr = (void *)crfss_get_block(sb, curr_p);
		type = nova_get_entry_type(addr);
		switch (type) {
			case SET_ATTR:
				curr_p += sizeof(struct nova_setattr_logentry);
				continue;
			case LINK_CHANGE:
				curr_p += sizeof(struct nova_link_change_entry);
				continue;
			case DIR_LOG:
				break;
			default:
				printk("%s: unknown type %d, 0x%llx\n",
							__func__, type, curr_p);
			BUG();
			return -EINVAL;
		}

		entry = (struct crfss_dentry *)crfss_get_block(sb, curr_p);
		printk("curr_p: 0x%llx, type %d, ino %llu, "
			"name %s, namelen %u, rec len %u\n", curr_p,
			entry->entry_type, le64_to_cpu(entry->ino),
			entry->name, entry->name_len,
			le16_to_cpu(entry->de_len));

		de_len = le16_to_cpu(entry->de_len);
		if (entry->ino > 0 && entry->invalid == 0) {
			ino = __le64_to_cpu(entry->ino);
			pos = BKDRHash(entry->name, entry->name_len);

			ret = crfss_get_inode_address(sb, ino, &pi_addr, 0);
			if (ret) {
				printk("%s: get child inode %lu address "
					"failed %d\n", __func__, ino, ret);
				ctx->pos = READDIR_END;
				return ret;
			}

			child_pi = crfss_get_block(sb, pi_addr);
			printk("ctx: ino %llu, name %s, "
				"name_len %u, de_len %u\n",
				(u64)ino, entry->name, entry->name_len,
				entry->de_len);
			if (prev_entry && !dir_emit(ctx, prev_entry->name,
				prev_entry->name_len, ino,
				IF2DT(le16_to_cpu(prev_child_pi->i_mode)))) {
				printk("Here: pos %llu\n", ctx->pos);
				return 0;
			}
			prev_entry = entry;
			prev_child_pi = child_pi;
		}
		ctx->pos = pos;
		curr_p += de_len;
	}

	if (prev_entry && !dir_emit(ctx, prev_entry->name,
			prev_entry->name_len, ino,
			IF2DT(le16_to_cpu(prev_child_pi->i_mode))))
		return 0;

	ctx->pos = READDIR_END;
out:
	//NOVA_END_TIMING(readdir_t, readdir_time);
	printk("%s return\n", __func__);
	return 0;
}

const struct file_operations nova_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.iterate	= nova_readdir,
	.fsync		= noop_fsync,
	.unlocked_ioctl = nova_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= nova_compat_ioctl,
#endif
};
#endif
