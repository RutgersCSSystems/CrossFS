/*
 * BRIEF DESCRIPTION
 *
 * Write protection for the filesystem pages.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include "pmfs.h"

DEFINE_SPINLOCK(crfs_writeable_lock);

static inline void wprotect_disable(void)
{
	unsigned long cr0_val;

	cr0_val = read_cr0();
	cr0_val &= (~X86_CR0_WP);
	write_cr0(cr0_val);
}

static inline void wprotect_enable(void)
{
	unsigned long cr0_val;

	cr0_val = read_cr0();
	cr0_val |= X86_CR0_WP;
	write_cr0(cr0_val);
}

/* FIXME: Use PAGE RW Bit */
int crfs_writeable_old(void *vaddr, unsigned long size, int rw)
{
	int ret = 0;
	unsigned long nrpages = size >> PAGE_SHIFT;
	unsigned long addr = (unsigned long)vaddr;

	/* Page aligned */
	addr &= PAGE_MASK;

	if (size & (PAGE_SIZE - 1))
		nrpages++;

	if (rw)
		ret = set_memory_rw(addr, nrpages);
	else
		ret = set_memory_ro(addr, nrpages);

	BUG_ON(ret);
	return 0;
}

/* FIXME: Assumes that we are always called in the right order.
 * crfs_writeable(vaddr, size, 1);
 * crfs_writeable(vaddr, size, 0);
 */
int crfs_writeable(void *vaddr, unsigned long size, int rw)
{
	static unsigned long flags;
	if (rw) {
		local_irq_save(flags);
		wprotect_disable();
	} else {
		wprotect_enable();
		local_irq_restore(flags);
	}
	return 0;
}

int crfs_xip_mem_protect(struct super_block *sb, void *vaddr,
			  unsigned long size, int rw)
{
	if (!crfs_is_wprotected(sb))
		return 0;
	if (crfs_is_protected_old(sb))
		return crfs_writeable_old(vaddr, size, rw);
	return crfs_writeable(vaddr, size, rw);
}
