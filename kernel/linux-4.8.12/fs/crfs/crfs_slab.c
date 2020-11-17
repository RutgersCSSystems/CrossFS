
/*
 * DevFS: Loadable Device File System
 *
 * Copyright (C) 2017 Sudarsun Kannan.  All rights reserved.
 *     Author: Sudarsun Kannan <sudarsun.kannan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Derived from original ramfs:
 *
 * TODO: DEVFS description
 */

/* Disable this for user-space */
#include <linux/devfs.h>
#define _DEVFS_SLAB_KERNEL

#if defined(_DEVFS_SLAB_KERNEL)
/*TODO: Header cleanup*/
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/devfs_def.h>
#include <linux/math64.h>
#include <linux/kernel.h>
#include <linux/in.h>
#include <linux/log2.h>
#include <linux/file.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/nvme.h>
#include <linux/syscalls.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/ramfs.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/fsnotify.h>
#include <linux/mount.h>
#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/namei.h>
#include <linux/devfs.h>


#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>
#include <xen/xen.h>
#include <xen/interface/xen.h>
#include <xen/interface/memory.h>
#include <xen/balloon.h>
#include <xen/heteromem.h>
#include <xen/features.h>
#include <xen/page.h>

const int tab64[64] = {
    63,  0, 58,  1, 59, 47, 53,  2,
    60, 39, 48, 27, 54, 33, 42,  3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22,  4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16,  9, 12,
    44, 24, 15,  8, 23,  7,  6,  5};

int log2_64 (ssize_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((ssize_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

#if defined(_DEVFS_DEBUG)
#define BUG_ON_DEBUG(x) BUG_ON(x)
#else
#define BUG_ON_DEBUG(x) do{ \
 ; \
} while (0)
#endif



#else

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <math.h>
#include <assert.h>
#include "devfs_slab.h"

#endif

#define SLAB_DUMP_COLOURED

#ifdef SLAB_DUMP_COLOURED
# define GRAY(s)   "\033[1;30m" s "\033[0m"
# define RED(s)    "\033[0;31m" s "\033[0m"
# define GREEN(s)  "\033[0;32m" s "\033[0m"
# define YELLOW(s) "\033[1;33m" s "\033[0m"
#else
# define GRAY(s)   s
# define RED(s)    s
# define GREEN(s)  s
# define YELLOW(s) s
#endif

#define SLOTS_ALL_ZERO ((uint64_t) 0)
#define SLOTS_FIRST ((uint64_t) 1)
#define FIRST_FREE_SLOT(s) ((ssize_t) __builtin_ctzll(s))

#if defined(_DEVFS_SLAB_USER)
#define FREE_SLOTS(s) ((ssize_t) __builtin_popcountll(s))
#else
#define FREE_SLOTS(s) ((ssize_t) hweight_long(s))
#endif

#define ONE_USED_SLOT(slots, empty_slotmask) \
    ( \
        ( \
            (~(slots) & (empty_slotmask))       & \
            ((~(slots) & (empty_slotmask)) - 1)   \
        ) == SLOTS_ALL_ZERO \
    )

#define POWEROF2(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

#define LIKELY(exp) __builtin_expect(exp, 1)
#define UNLIKELY(exp) __builtin_expect(exp, 0)

ssize_t slab_pagesize;


#ifndef NDEBUG
static int slab_is_valid(const struct slab_chain *const sch)
{
    ssize_t head = 0;

    BUG_ON_DEBUG(!POWEROF2(slab_pagesize));
    BUG_ON_DEBUG(!POWEROF2(sch->slabsize));
    BUG_ON_DEBUG(!POWEROF2(sch->pages_per_alloc));

/*
#if defined(_DEVFS_SLAB_USER)
    printf("sch->itemcount %d \n",sch->itemcount);
#else
    printk(KERN_ALERT "%s:%d sch->itemcount %d \n",__FUNCTION__,__LINE__,
		sch->itemcount);
#endif
*/

    BUG_ON_DEBUG(!(sch->itemcount >= 2 && sch->itemcount <= 64));
    BUG_ON_DEBUG(!(sch->itemsize >= 1 && sch->itemsize <= SIZE_MAX));
    BUG_ON_DEBUG(!(sch->pages_per_alloc >= slab_pagesize));
    BUG_ON_DEBUG(!(sch->pages_per_alloc >= sch->slabsize));

    BUG_ON_DEBUG(!(offsetof(struct slab_header, data) +
        sch->itemsize * sch->itemcount <= sch->slabsize));

    BUG_ON_DEBUG(!(sch->empty_slotmask == ~SLOTS_ALL_ZERO >> (64 - sch->itemcount)));
    BUG_ON_DEBUG(!(sch->initial_slotmask == (sch->empty_slotmask ^ SLOTS_FIRST)));
    BUG_ON_DEBUG(!(sch->alignment_mask == ~(sch->slabsize - 1)));

    const struct slab_header *const heads[] =
        {sch->full, sch->empty, sch->partial};

    for (head = 0; head < 3; ++head) {
        const struct slab_header *prev = NULL, *slab;

        for (slab = heads[head]; slab != NULL; slab = slab->next) {

            if (prev == NULL)
                BUG_ON_DEBUG(!(slab->prev == NULL));
            else
                BUG_ON_DEBUG(!(slab->prev == prev));

            switch (head) {
            case 0:
                BUG_ON_DEBUG(!(slab->slots == SLOTS_ALL_ZERO));
                break;

            case 1:
                BUG_ON_DEBUG(!(slab->slots == sch->empty_slotmask));
                break;

            case 2:
                BUG_ON_DEBUG(!((slab->slots & ~sch->empty_slotmask) == SLOTS_ALL_ZERO));
                BUG_ON_DEBUG(!(FREE_SLOTS(slab->slots) >= 1));
                BUG_ON_DEBUG(!(FREE_SLOTS(slab->slots) < sch->itemcount));
                break;
            }

            if (slab->refcount == 0) {
                BUG_ON_DEBUG(!((uintptr_t) slab % sch->slabsize == 0));

                if (sch->slabsize >= slab_pagesize)
                    BUG_ON_DEBUG(!((uintptr_t) slab->page % sch->slabsize == 0));
                else
                    BUG_ON_DEBUG(!((uintptr_t) slab->page % slab_pagesize == 0));
            } else {
                if (sch->slabsize >= slab_pagesize)
                    BUG_ON_DEBUG(!((uintptr_t) slab % sch->slabsize == 0));
                else
                    BUG_ON_DEBUG(!((uintptr_t) slab % slab_pagesize == 0));
            }

            prev = slab;
        }
    }

    return 1;
}
#endif

static void fn (const void *item)
{
#if defined(_DEVFS_SLAB_USER)
    printf("func %.3f\n", *((double *) item));
#else
    printk("func %.3f\n", *((double *) item));
#endif
}

#if !defined(_DEVFS_SLAB_USER)
void slab_init(struct slab_chain *const sch, ssize_t itemsize)
#else
void slab_init(struct slab_chain *const sch, size_t itemsize)
#endif
{
    BUG_ON_DEBUG(!(sch != NULL));
    BUG_ON_DEBUG(!(itemsize >= 1 && itemsize <= SIZE_MAX));
    BUG_ON_DEBUG(!POWEROF2(slab_pagesize));

    sch->itemsize = itemsize;

    const ssize_t data_offset = offsetof(struct slab_header, data);
    const ssize_t least_slabsize = data_offset + 64 * sch->itemsize;
#if defined(_DEVFS_SLAB_USER)
    sch->slabsize = (ssize_t) 1 << (ssize_t) ceil(log2(least_slabsize));
    printf("least_slabsize %d, sch->slabsize %d log2 %d, CEILING %f\n", 
	least_slabsize, sch->slabsize, log2(least_slabsize), (float)CEILING(log2(least_slabsize)));
#else
    sch->slabsize = (ssize_t) 1 << (ssize_t) CEILING(ilog2(least_slabsize));
    //sch->slabsize = 1024;
    //printk(KERN_ALERT "least_slabsize %d, slabsize %d \n", least_slabsize, sch->slabsize);
    //printk(KERN_ALERT "log2_64 %d, CEILING %f\n", ilog2(least_slabsize), (float)CEILING(ilog2(least_slabsize)));

#endif
    sch->itemcount = 64;

    if (sch->slabsize - least_slabsize != 0) {
        const ssize_t shrinked_slabsize = sch->slabsize >> 1;

        if (data_offset < shrinked_slabsize &&
            shrinked_slabsize - data_offset >= 2 * sch->itemsize) {

            sch->slabsize = shrinked_slabsize;
            sch->itemcount = (shrinked_slabsize - data_offset) / sch->itemsize;
        }
    }

    sch->pages_per_alloc = sch->slabsize > slab_pagesize ?
        sch->slabsize : slab_pagesize;

    sch->empty_slotmask = ~SLOTS_ALL_ZERO >> (64 - sch->itemcount);
    sch->initial_slotmask = sch->empty_slotmask ^ SLOTS_FIRST;
    sch->alignment_mask = ~(sch->slabsize - 1);
    sch->partial = sch->empty = sch->full = NULL;

#if defined(_DEVFS_SLAB_USER)
    printf("slab_init sch->itemsize %d slab_pagesize %u "
		"sch->slabsize %u  sch->itemcount %d\n", sch->itemsize, 
		slab_pagesize, sch->slabsize,  sch->itemcount);
#else
    printk(KERN_ALERT "slab_init sch->itemsize %d slab_pagesize %u "
		"sch->slabsize %u  sch->itemcount %d\n", sch->itemsize, 
		slab_pagesize, sch->slabsize,  sch->itemcount);
#endif

    BUG_ON_DEBUG(!slab_is_valid(sch));

    return;


}

/* Note that alignment must be a power of two. */
void * allocate_aligned(size_t size, size_t alignment, unsigned long ptr)
{
  const size_t mask = alignment - 1;
  //const uintptr_t mem = (uintptr_t) malloc(size + alignment);
  const uintptr_t mem = (uintptr_t)ptr;
  return (void *) ((mem + mask) & ~mask);
}

void *crfss_slab_alloc(struct crfss_sb_info *sbi, struct slab_chain *sch, 
		void **start_addr, unsigned long *off)
{
    BUG_ON_DEBUG(!(sch != NULL));
    BUG_ON_DEBUG(!(slab_is_valid(sch)));

    if (LIKELY(sch->partial != NULL)) {
        /* found a partial slab, locate the first free slot */
        register const ssize_t slot = FIRST_FREE_SLOT(sch->partial->slots);
        sch->partial->slots ^= SLOTS_FIRST << slot;

        if (UNLIKELY(sch->partial->slots == SLOTS_ALL_ZERO)) {
            /* slab has become full, change state from partial to full */
            struct slab_header *const tmp = sch->partial;

            /* skip first slab from partial list */
            if (LIKELY((sch->partial = sch->partial->next) != NULL))
                sch->partial->prev = NULL;

            if (LIKELY((tmp->next = sch->full) != NULL))
                sch->full->prev = tmp;

            sch->full = tmp;
            return sch->full->data + slot * sch->itemsize;
        } else {
            return sch->partial->data + slot * sch->itemsize;
        }
    } else if (LIKELY((sch->partial = sch->empty) != NULL)) {
        /* found an empty slab, change state from empty to partial */
        if (LIKELY((sch->empty = sch->empty->next) != NULL))
            sch->empty->prev = NULL;

        sch->partial->next = NULL;

        /* slab is located either at the beginning of page, or beyond */
        UNLIKELY(sch->partial->refcount != 0) ?
            sch->partial->refcount++ : sch->partial->page->refcount++;

        sch->partial->slots = sch->initial_slotmask;
        return sch->partial->data;

    } else {

        /* no empty or partial slabs available, create a new one */
        if (sch->slabsize <= slab_pagesize) {

#if 1 //defined(_DEVFS_SLAB_USER)
            /*sch->partial = mmap(NULL, sch->pages_per_alloc,
                PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (UNLIKELY(sch->partial == MAP_FAILED))
                return perror("mmap"), sch->partial = NULL;*/
    	    sch->partial = allocate_aligned(sch->pages_per_alloc, PAGE_SIZE, *start_addr);
    	    *start_addr = *start_addr + PAGE_SIZE + sch->pages_per_alloc;
	    *off = *off +  PAGE_SIZE + sch->pages_per_alloc;
#else
	    sch->partial = allocate_aligned(sch->pages_per_alloc, PAGE_SIZE, sbi->i_host_addr);
	    sbi->i_host_addr = sbi->i_host_addr + PAGE_SIZE + sch->pages_per_alloc;
	    sbi->i_host_off = sbi->i_host_off + PAGE_SIZE + sch->pages_per_alloc;
#endif
        } else {

#if 1 //defined(_DEVFS_SLAB_USER)
          /*const int err = posix_memalign((void **) &sch->partial,
                sch->slabsize, sch->pages_per_alloc);

            if (UNLIKELY(err != 0)) {

                return sch->partial = NULL;
            }
            printf("posix_memalign(align=%zu, size=%zu), host off %zu\n",
                    sch->slabsize, sch->pages_per_alloc, sbi->i_host_off);*/

    	    sch->partial = allocate_aligned(sch->pages_per_alloc, sch->slabsize, *start_addr);
    	    *start_addr = *start_addr + sch->slabsize + sch->pages_per_alloc;
            *off = *off + sch->slabsize + sch->pages_per_alloc;
#else
	    BUG_ON_DEBUG(1);
#endif
            }

        struct slab_header *prev = NULL;

        const char *const page_end =
            (char *) sch->partial + sch->pages_per_alloc;

        union {
            const char *c;
            struct slab_header *const s;
        } curr = {
            .c = (const char *) sch->partial + sch->slabsize
        };

        __builtin_prefetch(sch->partial, 1);

        sch->partial->prev = sch->partial->next = NULL;
        sch->partial->refcount = 1;
        sch->partial->slots = sch->initial_slotmask;

        if (LIKELY(curr.c != page_end)) {
            curr.s->prev = NULL;
            curr.s->refcount = 0;
            curr.s->page = sch->partial;
            curr.s->slots = sch->empty_slotmask;
            sch->empty = prev = curr.s;

            while (LIKELY((curr.c += sch->slabsize) != page_end)) {
                prev->next = curr.s;
                curr.s->prev = prev;
                curr.s->refcount = 0;
                curr.s->page = sch->partial;
                curr.s->slots = sch->empty_slotmask;
                prev = curr.s;
            }

            prev->next = NULL;
        }

        return sch->partial->data;
    }

    /* unreachable */
}

void slab_free(struct slab_chain *const sch, const void *const addr)
{

    struct slab_header *const slab = (void *)((uintptr_t) addr & sch->alignment_mask);

    BUG_ON_DEBUG(!(sch != NULL));
    BUG_ON_DEBUG(!(slab_is_valid(sch)));
    BUG_ON_DEBUG(!(addr != NULL));

    const int slot = ((char *) addr - (char *) slab -
        offsetof(struct slab_header, data)) / sch->itemsize;

    if (UNLIKELY(slab->slots == SLOTS_ALL_ZERO)) {
        /* target slab is full, change state to partial */
        slab->slots = SLOTS_FIRST << slot;

        if (LIKELY(slab != sch->full)) {
            if (LIKELY((slab->prev->next = slab->next) != NULL))
                slab->next->prev = slab->prev;

            slab->prev = NULL;
        } else if (LIKELY((sch->full = sch->full->next) != NULL)) {
            sch->full->prev = NULL;
        }

        slab->next = sch->partial;

        if (LIKELY(sch->partial != NULL))
            sch->partial->prev = slab;

        sch->partial = slab;

    } else if (UNLIKELY(ONE_USED_SLOT(slab->slots, sch->empty_slotmask))) {

        /* target slab is partial and has only one filled slot */
        if (UNLIKELY(slab->refcount == 1 || (slab->refcount == 0 &&
            slab->page->refcount == 1))) {

            /* unmap the whole page if this slab is the only partial one */
            if (LIKELY(slab != sch->partial)) {
                if (LIKELY((slab->prev->next = slab->next) != NULL))
                    slab->next->prev = slab->prev;
            } else if (LIKELY((sch->partial = sch->partial->next) != NULL)) {
                sch->partial->prev = NULL;
            }

            void *const page = UNLIKELY(slab->refcount != 0) ? slab : slab->page;
            const char *const page_end = (char *) page + sch->pages_per_alloc;
            char found_head = 0;

            union {
                const char *c;
                const struct slab_header *const s;
            } s;

            for (s.c = page; s.c != page_end; s.c += sch->slabsize) {
                if (UNLIKELY(s.s == sch->empty))
                    found_head = 1;
                else if (UNLIKELY(s.s == slab))
                    continue;
                else if (LIKELY((s.s->prev->next = s.s->next) != NULL))
                    s.s->next->prev = s.s->prev;
            }

            if (UNLIKELY(found_head && (sch->empty = sch->empty->next) != NULL))
                sch->empty->prev = NULL;


#if defined(_DEVFS_SLAB_USER)
            /*if (sch->slabsize <= slab_pagesize) {
                if (UNLIKELY(munmap(page, sch->pages_per_alloc) == -1))
                    perror("munmap");
            } else {
                free(page);
            }*/
#endif
        } else {

            slab->slots = sch->empty_slotmask;

            if (LIKELY(slab != sch->partial)) {
                if (LIKELY((slab->prev->next = slab->next) != NULL))
                    slab->next->prev = slab->prev;

                slab->prev = NULL;
            } else if (LIKELY((sch->partial = sch->partial->next) != NULL)) {
                sch->partial->prev = NULL;
            }

            slab->next = sch->empty;

            if (LIKELY(sch->empty != NULL))
                sch->empty->prev = slab;

            sch->empty = slab;

            UNLIKELY(slab->refcount != 0) ?
                slab->refcount-- : slab->page->refcount--;
        }
    } else {
        /* target slab is partial, no need to change state */
        slab->slots |= SLOTS_FIRST << slot;
    }
}


void slab_traverse(const struct slab_chain *const sch, void (*fn)(const void *))
{

    const struct slab_header *slab;
    const char *item, *end;
    uint64_t mask = 0;

    BUG_ON_DEBUG(!(sch != NULL));
    BUG_ON_DEBUG(!(fn != NULL));
    BUG_ON_DEBUG(!(slab_is_valid(sch)));

    const ssize_t data_offset = offsetof(struct slab_header, data);

    for (slab = sch->partial; slab; slab = slab->next) {
        item = (const char *) slab + data_offset;
        end = item + sch->itemcount * sch->itemsize;
        mask = SLOTS_FIRST;

        do {
            if (!(slab->slots & mask))
                fn(item);

            mask <<= 1;
        } while ((item += sch->itemsize) != end);
    }

    for (slab = sch->full; slab; slab = slab->next) {
        item = (const char *) slab + data_offset;
        end = item + sch->itemcount * sch->itemsize;

        do fn(item);
        while ((item += sch->itemsize) != end);
    }
}

void slab_destroy(const struct slab_chain *const sch)
{
    BUG_ON_DEBUG(!(sch != NULL));
    BUG_ON_DEBUG(!slab_is_valid(sch));

    struct slab_header *const heads[] = {sch->partial, sch->empty, sch->full};
    struct slab_header *pages_head = NULL, *pages_tail;
    ssize_t i = 0;

    for (i = 0; i < 3; ++i) {
        struct slab_header *slab = heads[i];

        while (slab != NULL) {
            if (slab->refcount != 0) {
                struct slab_header *const page = slab;
                slab = slab->next;

                if (UNLIKELY(pages_head == NULL))
                    pages_head = page;
                else
                    pages_tail->next = page;

                pages_tail = page;
            } else {
                slab = slab->next;
            }
        }
    }

    if (LIKELY(pages_head != NULL)) {
        pages_tail->next = NULL;
        struct slab_header *page = pages_head;

        if (sch->slabsize <= slab_pagesize) {
            do {
                page = page->next;

#if defined(_DEVFS_SLAB_USER)
                void *const target = page;
                //if (UNLIKELY(munmap(target, sch->pages_per_alloc) == -1))
                  //  perror("munmap");
#endif
            } while (page != NULL);
        } else {

            do {
                page = page->next;
#if defined(_DEVFS_SLAB_USER)
                void *const target = page;
                //free(target);
#endif
            } while (page != NULL);
        }
    }
}


#if defined(_DEVFS_SLAB_TESTING)

int crfss_slab_test(struct crfss_sb_info *sbi)
{
    struct slab_chain s;
    ssize_t i = 0;
    ssize_t j = 0;
    double **allocs;
    j = sizeof(double *) * _DEVFS_TEST_ITR;

#if defined(_DEVFS_SLAB_USER)
    slab_pagesize = (size_t) sysconf(_SC_PAGESIZE);
#else
    slab_pagesize = (ssize_t) PAGE_SIZE;
#endif
    //slab_init(&s, sizeof(struct inode));
    slab_init(&s, 1024);


#if defined(_DEVFS_SLAB_USER)
    struct crfss_sb_info sbi1;
    sbi1.i_host_addr = (void *)malloc(2097152);
    sbi1.i_host_off = 0;

    allocs = malloc(j);
#else
    allocs = kmalloc(j, GFP_KERNEL);
#endif
    BUG_ON_DEBUG(!allocs);

    for (i = 0; i < _DEVFS_TEST_ITR; i++) {

#if defined(_DEVFS_SLAB_USER)
	allocs[i] = crfss_slab_alloc(&sbi1, &s, &sbi1->i_host_addr, &sbi1->i_host_off);
#else
        allocs[i] = crfss_slab_alloc(sbi, &s, &sbi->i_host_addr, &sbi->i_host_off);
#endif
        BUG_ON_DEBUG(!(allocs[i] != NULL));
        //*allocs[i] = i * 4;
        memset(allocs[i], 0, 1024);
        //SLAB_DUMP;

#if defined(_DEVFS_SLAB_USER)
	printf("crfss_slab_init finished allocation %d \n", i );
#else
    printk(KERN_ALERT "crfss_slab_init finished allocation %d \n", i );
#endif

    }

    for (j = _DEVFS_TEST_ITR - 1; j >= 0; j--) {
        slab_free(&s, allocs[j]);
        //SLAB_DUMP;
    }
    slab_destroy(&s);

    return 0;
}
#endif

/*Initialize DevFS cache object*/
struct slab_chain *crfss_slab_init(struct crfss_sb_info *sbi, ssize_t size)
{
    struct slab_chain *s = NULL;
#if defined(_DEVFS_SLAB_USER)
    s = malloc(sizeof(struct slab_chain));
#else
    s = kmalloc(sizeof(struct slab_chain), GFP_KERNEL);
#endif
    BUG_ON_DEBUG(!s);
    slab_pagesize = (ssize_t) PAGE_SIZE;
    slab_init(s, size);
    //slab_init(s, 1024);

#if defined(_DEVFS_SLAB_TESTING)
    crfss_slab_test(sbi);
#endif
    return s;
}


struct slab_chain *crfss_slab_free(struct crfss_sb_info *sbi, struct slab_chain *s)
{

    BUG_ON_DEBUG(!s);

    //TODO Clean up all slab entries

#if defined(_DEVFS_SLAB_USER)
    free(s);    
#else
    kfree(s);
#endif
    return s;
}



#if defined(_DEVFS_SLAB_USER)
int main(void)
{
#if defined(_DEVFS_SLAB_TESTING)
	crfss_slab_test(NULL);
#endif
    return 0;
}
#endif


#if 0
static void slab_dump(FILE *const out, const struct slab_chain *const sch)
{
    BUG_ON_DEBUG(out != NULL);
    BUG_ON_DEBUG(sch != NULL);
    BUG_ON_DEBUG(slab_is_valid(sch));

    const struct slab_header *const heads[] =
        {sch->partial, sch->empty, sch->full};

    const char *labels[] = {"part", "empt", "full"};

    for (ssize_t i = 0; i < 3; ++i) {
        const struct slab_header *slab = heads[i];

        fprintf(out,
            YELLOW("%6s ") GRAY("|%2d%13s|%2d%13s|%2d%13s|%2d%13s") "\n",
            labels[i], 64, "", 48, "", 32, "", 16, "");

        unsigned long long total = 0, row;

        for (row = 1; slab != NULL; slab = slab->next, ++row) {
            const unsigned used = sch->itemcount - FREE_SLOTS(slab->slots);
            fprintf(out, GRAY("%6llu "), row);

            for (int k = 63; k >= 0; --k) {
                fprintf(out, slab->slots & (SLOTS_FIRST << k) ? GREEN("1") :
                    ((ssize_t) k >= sch->itemcount ? GRAY("0") : RED("0")));
            }

            fprintf(out, RED(" %8u") "\n", used);
            total += used;
        }

        fprintf(out,
            GREEN("%6s ") GRAY("^%15s^%15s^%15s^%15s") YELLOW(" %8llu") "\n\n",
            "", "", "", "", "", total);
    }
}

static void slab_stats(FILE *const out, const struct slab_chain *const sch)
{
    BUG_ON_DEBUG(out != NULL);
    BUG_ON_DEBUG(sch != NULL);
    BUG_ON_DEBUG(slab_is_valid(sch));

    long long unsigned
        total_nr_slabs = 0,
        total_used_slots = 0,
        total_free_slots = 0;

    float occupancy;

    const struct slab_header *const heads[] =
        {sch->partial, sch->empty, sch->full};

    const char *labels[] = {"Partial", "Empty", "Full"};

    fprintf(out, "%8s %17s %17s %17s %17s\n", "",
        "Slabs", "Used", "Free", "Occupancy");

    for (ssize_t i = 0; i < 3; ++i) {
        long long unsigned nr_slabs = 0, used_slots = 0, free_slots = 0;
        const struct slab_header *slab;

        for (slab = heads[i]; slab != NULL; slab = slab->next) {
            nr_slabs++;
            used_slots += sch->itemcount - FREE_SLOTS(slab->slots);
            free_slots += FREE_SLOTS(slab->slots);
        }

        occupancy = used_slots + free_slots ?
            100 * (float) used_slots / (used_slots + free_slots) : 0.0;

        fprintf(out, "%8s %17llu %17llu %17llu %16.2f%%\n",
            labels[i], nr_slabs, used_slots, free_slots, occupancy);

        total_nr_slabs += nr_slabs;
        total_used_slots += used_slots;
        total_free_slots += free_slots;
    }

    occupancy = total_used_slots + total_free_slots ?
        100 * (float) total_used_slots / (total_used_slots + total_free_slots) :
        0.0;

    fprintf(out, "%8s %17llu %17llu %17llu %16.2f%%\n", "Total",
        total_nr_slabs, total_used_slots, total_free_slots, occupancy);
}

static void slab_props(FILE *const out, const struct slab_chain *const sch)
{
    BUG_ON_DEBUG(out != NULL);
    BUG_ON_DEBUG(sch != NULL);
    BUG_ON_DEBUG(slab_is_valid(sch));

    fprintf(out,
        "%18s: %8zu\n"
        "%18s: %8zu = %.2f * (%zu pagesize)\n"
        "%18s: %8zu = (%zu offset) + (%zu itemcount) * (%zu itemsize)\n"
        "%18s: %8zu = (%zu slabsize) - (%zu total)\n"
        "%18s: %8zu = %zu * (%zu pagesize)\n"
        "%18s: %8zu = (%zu alloc) / (%zu slabsize)\n",

        "pagesize",
            slab_pagesize,

        "slabsize",
            sch->slabsize, (float) sch->slabsize / slab_pagesize, slab_pagesize,

        "total",
            offsetof(struct slab_header, data) + sch->itemcount * sch->itemsize,
            offsetof(struct slab_header, data), sch->itemcount, sch->itemsize,

        "waste per slab",
            sch->slabsize - offsetof(struct slab_header, data) -
            sch->itemcount * sch->itemsize, sch->slabsize,
            offsetof(struct slab_header, data) + sch->itemcount * sch->itemsize,

        "pages per alloc",
            sch->pages_per_alloc, sch->pages_per_alloc / slab_pagesize,
            slab_pagesize,

        "slabs per alloc",
            sch->pages_per_alloc / sch->slabsize, sch->pages_per_alloc,
            sch->slabsize
    );
}


#define SLAB_DUMP { \
    puts("\033c"); \
    slab_props(stdout, &s); \
    puts(""); \
    slab_stats(stdout, &s); \
    puts(""); \
    slab_dump(stdout, &s); \
    fflush(stdout); \
    usleep(100000); \
}

#endif
