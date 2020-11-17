/******************************************************************************
 * Xen heteromem functionality
 */

#include <asm/xen/interface.h>

#define RETRY_UNLIMITED	0
//#define PAGE_MIGRATED 111

struct heteromem_stats {
	/* We aim for 'current allocation' == 'target allocation'. */
	unsigned long current_pages;
	unsigned long target_pages;
	/* Number of pages in high- and low-memory heteromems. */
	unsigned long heteromem_low;
	unsigned long heteromem_high;
	unsigned long schedule_delay;
	unsigned long max_schedule_delay;
	unsigned long retry_count;
	unsigned long max_retry_count;
#ifdef CONFIG_XEN_BALLOON_MEMORY_HOTPLUG
	unsigned long hotplug_pages;
	unsigned long heteromem_hotplug;
#endif
};

extern struct heteromem_stats heteromem_stats;

void heteromem_set_new_target(unsigned long target);

int alloc_xenheteromemed_pages(int nr_pages, struct page **pages,
		bool highmem, int delpage);
void free_xenheteromemed_pages(int nr_pages, struct page **pages);
void heteromem_append(struct page *page);
int add_readylist_setup(struct page *page);
int heteromem_init(int idx, unsigned long start, unsigned long size);

/*HeteroMem Get next page*/
struct page *hetero_getnxt_page(bool prefer_highmem);
struct page *hetero_getnxt_io_page(bool prefer_highmem);
struct page *hetero_alloc_hetero(gfp_t gfp, int order, int node);
struct page *hetero_alloc_migrate(gfp_t gfp, int order, int node);
struct page *hetero_alloc_IO(gfp_t gfp, int order, int node);
void hetero_add_to_nvlist(struct page *page);

void hetero_free_hetero(void);
void increment_hetero_alloc_hit(void);
void increment_hetero_alloc_miss(void);

int send_hotpage_skiplist(void);
xen_pfn_t *get_hotpage_list(unsigned int *hotcnt);
int is_hetero_hot_page(struct page *page);
/* heteromem function when applications exits*/
int heteromem_app_exit(void);
/*heteromem application enter*/
int heteromem_app_enter(unsigned long arg,
                         unsigned int hot_scan_freq,
                         unsigned int hot_scan_limit,
                         unsigned int hot_shrink_freq,
                         unsigned int usesharedmem,
						 unsigned int maxfastmempgs);

struct device;
#ifdef CONFIG_XEN_SELFBALLOONING
extern int register_xen_selfheteromeming(struct device *dev);
#else
static inline int register_xen_selfheteromeming(struct device *dev)
{
	return -ENOSYS;
}
#endif
