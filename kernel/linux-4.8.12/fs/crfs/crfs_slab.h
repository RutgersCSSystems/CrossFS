#ifndef _LINUX_DEVFS_SLAB_H
#define _LINUX_DEVFS_SLAB_H

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/blockgroup_lock.h>
#include <linux/percpu_counter.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/circ_buf.h>
#include <linux/devfs.h>
#include <linux/devfs_def.h>

/*  Implement ceiling, floor functions.  */
#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) ((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )

//#define _DEVFS_SLAB_USER
#define _DEVFS_SLAB_TESTING
#define _DEVFS_TEST_ITR 512
//#define PAGE_SIZE 4096

extern ssize_t slab_pagesize;

struct slab_header {
    struct slab_header *prev, *next;
    uint64_t slots;
    uintptr_t refcount;
    struct slab_header *page;
    uint8_t data[] __attribute__((aligned(sizeof(void *))));
};

struct slab_chain {
    ssize_t itemsize, itemcount;
    ssize_t slabsize, pages_per_alloc;
    uint64_t initial_slotmask, empty_slotmask;
    uintptr_t alignment_mask;
    struct slab_header *partial, *empty, *full;
};

/*Initialize DevFS cache object*/
int crfss_slab_init(struct crfss_sb_info *sbi, struct slab_chain *s);
void slab_init(struct slab_chain *, ssize_t);
void *crfss_slab_alloc(struct crfss_sb_info *sbi, struct slab_chain *sch,
                void **start_addr, unsigned long *off);
void crfss_slab_free(struct slab_chain *, const void *);
void slab_traverse(const struct slab_chain *, void (*)(const void *));
void slab_destroy(const struct slab_chain *);

#endif
