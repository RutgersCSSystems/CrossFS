/*
 * BRIEF DESCRIPTION
 *
 * Ioctl operations.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2010-2011 Marco Stornelli <marco.stornelli@gmail.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * change log:
 * inode->i_mutex -> inode->i_rwsem
 */

#include <linux/capability.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/mount.h>
#include "pmfs.h"

long crfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct crfs_inode *pi;
	struct super_block *sb = inode->i_sb;
	unsigned int flags;
	int ret;
	crfs_transaction_t *trans;

	pi = crfs_get_inode(sb, inode->i_ino);
	if (!pi)
		return -EACCES;

	switch (cmd) {
	case FS_IOC_GETFLAGS:
		flags = le32_to_cpu(pi->i_flags) & PMFS_FL_USER_VISIBLE;
		return put_user(flags, (int __user *)arg);
	case FS_IOC_SETFLAGS: {
		unsigned int oldflags;

		ret = mnt_want_write_file(filp);
		if (ret)
			return ret;

		if (!inode_owner_or_capable(inode)) {
			ret = -EPERM;
			goto flags_out;
		}

		if (get_user(flags, (int __user *)arg)) {
			ret = -EFAULT;
			goto flags_out;
		}

		//mutex_lock(&inode->i_mutex);
		inode_crfss_lock(inode);
		oldflags = le32_to_cpu(pi->i_flags);

		if ((flags ^ oldflags) &
		    (FS_APPEND_FL | FS_IMMUTABLE_FL)) {
			if (!capable(CAP_LINUX_IMMUTABLE)) {
				//mutex_unlock(&inode->i_mutex);
				inode_crfss_unlock(inode);
				ret = -EPERM;
				goto flags_out;
			}
		}

		if (!S_ISDIR(inode->i_mode))
			flags &= ~FS_DIRSYNC_FL;

		flags = flags & FS_FL_USER_MODIFIABLE;
		flags |= oldflags & ~FS_FL_USER_MODIFIABLE;
		inode->i_ctime = CURRENT_TIME_SEC;
		trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			goto out;
		}
		crfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);
		crfs_memunlock_inode(sb, pi);
		pi->i_flags = cpu_to_le32(flags);
		pi->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
		crfs_set_inode_flags(inode, pi);
		crfs_memlock_inode(sb, pi);
		crfs_commit_transaction(sb, trans);
out:
		//mutex_unlock(&inode->i_mutex);
		inode_crfss_unlock(inode);
flags_out:
		mnt_drop_write_file(filp);
		return ret;
	}
	case FS_IOC_GETVERSION:
		return put_user(inode->i_generation, (int __user *)arg);
	case FS_IOC_SETVERSION: {
		__u32 generation;
		if (!inode_owner_or_capable(inode))
			return -EPERM;
		ret = mnt_want_write_file(filp);
		if (ret)
			return ret;
		if (get_user(generation, (int __user *)arg)) {
			ret = -EFAULT;
			goto setversion_out;
		}
		//mutex_lock(&inode->i_mutex);
		inode_crfss_lock(inode);

		trans = crfs_new_transaction(sb, MAX_INODE_LENTRIES);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			goto out;
		}
		crfs_add_logentry(sb, trans, pi, sizeof(*pi), LE_DATA);
		inode->i_ctime = CURRENT_TIME_SEC;
		inode->i_generation = generation;
		crfs_memunlock_inode(sb, pi);
		pi->i_ctime = cpu_to_le32(inode->i_ctime.tv_sec);
		pi->i_generation = cpu_to_le32(inode->i_generation);
		crfs_memlock_inode(sb, pi);
		crfs_commit_transaction(sb, trans);

		//mutex_unlock(&inode->i_mutex);
		inode_crfss_unlock(inode);
setversion_out:
		mnt_drop_write_file(filp);
		return ret;
	}
	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long crfs_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FS_IOC32_GETFLAGS:
		cmd = FS_IOC_GETFLAGS;
		break;
	case FS_IOC32_SETFLAGS:
		cmd = FS_IOC_SETFLAGS;
		break;
	case FS_IOC32_GETVERSION:
		cmd = FS_IOC_GETVERSION;
		break;
	case FS_IOC32_SETVERSION:
		cmd = FS_IOC_SETVERSION;
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return crfs_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif
