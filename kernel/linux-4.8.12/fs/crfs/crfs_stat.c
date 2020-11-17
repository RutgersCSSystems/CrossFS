/*
 * devfs_stat.c
 *
 * Description: Gather statistics of FirmFS
 *
 */

#include <linux/fs.h>
#include <linux/devfs.h>

static atomic_t fp_queue_access_cnt = ATOMIC_INIT(0);
static atomic_t fp_queue_hit_cnt = ATOMIC_INIT(0);
static atomic_t fp_queue_conflict_cnt = ATOMIC_INIT(0);
static atomic_t write_fin_cnt = ATOMIC_INIT(0);

/*
 * File pointer queue hit stat
 */
void crfss_stat_fp_queue_init(void) {
	atomic_set(&fp_queue_access_cnt, 0);	
	atomic_set(&fp_queue_hit_cnt, 0);	
	atomic_set(&fp_queue_conflict_cnt, 0);	
	atomic_set(&write_fin_cnt, 0);	
}

void crfss_stat_fp_queue_access(void) {
	atomic_inc(&fp_queue_access_cnt);	
}

void crfss_stat_fp_queue_hit(void) {
	atomic_inc(&fp_queue_hit_cnt);	
}

void crfss_stat_fp_queue_conflict(void) {
	atomic_inc(&fp_queue_conflict_cnt);	
}

void crfss_stat_write_fin(void) {
	atomic_inc(&write_fin_cnt);	
}

int crfss_stat_get_write_fin(void) {
	return atomic_read(&write_fin_cnt);	
}

void crfss_stat_fp_queue_count(void) {
	int access_count = atomic_read(&fp_queue_access_cnt);
	int hit_count = atomic_read(&fp_queue_hit_cnt);
	int conflict_count = atomic_read(&fp_queue_conflict_cnt);
	int write_count = atomic_read(&write_fin_cnt);
	printk(KERN_ALERT "queue access count = %d\n", access_count);
	printk(KERN_ALERT "queue hit count = %d\n", hit_count);
	printk(KERN_ALERT "queue conflict count = %d\n", conflict_count);
	printk(KERN_ALERT "total write count = %d\n", write_count);
	crfss_stat_fp_queue_init();
}

/*
 * File descriptor count stat
 */
int crfss_stat_rd_count(void) {
	int rd_count = 0;
	struct crfss_fstruct *rd = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(rd, g_rd_list, list) {
		++rd_count;	
	}
	rcu_read_unlock();

	printk(KERN_ALERT "rd count = %d\n", rd_count);
	return 0;
}
