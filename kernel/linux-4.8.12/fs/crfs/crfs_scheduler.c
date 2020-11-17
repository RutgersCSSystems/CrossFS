/*
 * crfss_scheduler.c
 *
 * Description: Scheduler related functions
 *
 */
#include <linux/fs.h>
#include <linux/devfs.h>
#include <linux/file.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/nvme.h>
#include <linux/mm.h>

#define READ_TIMEOUT	500	// 500us 
#define WRITE_TIMEOUT	5000	// 5000us
#define KTHREAD_TIMEOUT 500000  // 50ms

#define DEVFS_SCHED_ROUND_ROBIN	0
#define DEVFS_SCHED_READ_PRIO	1
#define DEVFS_SCHED_PER_THREAD_RD_LIST 2

#define DEV_THREAD_IDLE 0x10
#define DEV_THREAD_EXIT 0x20

#define _DEVFS_THREAD_AFFNITY 10

/* Global Variable Definitions */
#ifdef CRFS_MULTI_PROC
int crfss_device_thread_nr[HOST_PROCESS_MAX];
struct dev_thread_struct crfss_device_thread[HOST_PROCESS_MAX][DEVICE_THREAD_MAX];
#else
int crfss_device_thread_nr;
struct dev_thread_struct crfss_device_thread[DEVICE_THREAD_MAX];
#endif
int crfss_scheduler_policy;
struct crfss_fstruct g_dummy_rd;
struct crfss_fstruct control_rd;
struct list_head *g_rd_list;
struct mutex g_mutex;

int g_dev_thread_round = 0;

/* 
 * Initialize scheduler
 *
 * Called when file system is mounted
 */

void crfss_loop(void *data) {
	unsigned long j0, j1;
	int delay = 60*HZ;

	while(1) {
		cond_resched();
	}
	j0 = jiffies;
	j1 = j0 + delay;
	while (time_before(jiffies, j1)) {
		//printk(KERN_ALERT "testing...\n");
		schedule();
	}
}

int crfss_scheduler_init(int nr, int policy) {
	int retval = 0, i = 0;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	/* init in-device kthread handling I/O request */
	if (nr > DEVICE_THREAD_MAX) {
		printk(KERN_ALERT "DEBUG: device thread limit exceed! | %s:%d",
			__FUNCTION__, __LINE__);
	}

#ifdef CRFS_MULTI_PROC
	crfss_device_thread_nr[process_idx] = nr;
#else
	crfss_device_thread_nr = nr;
#endif

	/* initialize scheduling policy */
	crfss_scheduler_policy = policy;

	/* initialize global rd list mutex */
	crfss_mutex_init(&g_mutex);

	/* initialize dummy rd */
	g_dummy_rd.state = DEVFS_RD_BUSY;
	g_dummy_rd.closed = 0;

	/* initialize file pointer queue list */
	g_rd_list = &g_dummy_rd.list;
	INIT_LIST_HEAD(g_rd_list);

#ifndef CRFS_PERF_TUNING
	/* initialize control-plane rd */
	control_rd.closed = 0;
	crfss_mutex_init(&control_rd.read_lock);

	control_rd.io_done = 0;
	control_rd.state = DEVFS_RD_IDLE;

	control_rd.qentrycnt = 32;
	control_rd.queuesize = QUEUESZ(control_rd.qentrycnt);
	control_rd.entrysize = sizeof(__u64);

	control_rd.fifo.buf = kzalloc(QUEUESZ(control_rd.qentrycnt), GFP_KERNEL);
	if (!control_rd.fifo.buf) {
		printk(KERN_ALERT "DEBUG: Failed %s:%d \n",__FUNCTION__,__LINE__);
	}
	control_rd.fifo.head = 0;
	control_rd.fifo.tail = 0;
	control_rd.qnumblocks = control_rd.qentrycnt;

	INIT_LIST_HEAD(&control_rd.list);
	control_rd.req = NULL;
	list_add(&control_rd.list, g_rd_list);

	crfss_dbg("g_rd_list = %llx, dev core cnt = %d, policy = %d\n", 
		(long long unsigned int)g_rd_list, crfss_device_thread_nr, policy);
#endif

	/* initialize device thread structure */
	for (i = 0; i < nr; ++i) {

#ifdef CRFS_MULTI_PROC
		/* initialize per device thread rd list */
		crfss_device_thread[process_idx][i].dummy_rd.state = DEVFS_RD_BUSY;
		crfss_device_thread[process_idx][i].dummy_rd.closed = 0;
		INIT_LIST_HEAD(&crfss_device_thread[process_idx][i].dummy_rd.list);

		/* initialize state */
		crfss_device_thread[process_idx][i].state = DEV_THREAD_IDLE;

		/* initialize mutex */
		crfss_mutex_init(&crfss_device_thread[process_idx][i].per_thread_mutex);

		/* initialize current serving queue */
		crfss_device_thread[process_idx][i].current_rd_list = NULL;

		/* initialize number of active file pointers */
		crfss_device_thread[process_idx][i].rd_nr = 0;

#ifndef CRFS_SCHED_THREAD
		/* create kthread to simulate device thread */
		crfss_device_thread[process_idx][i].kthrd = kthread_create(crfss_io_scheduler,
						&crfss_device_thread[i], "kdevio");

		if (IS_ERR(crfss_device_thread[process_idx][i].kthrd)) {
			printk(KERN_ALERT "%s:%d Failed kthread create \n",
			__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_scheduler_init;
		}
		kthread_bind(crfss_device_thread[process_idx][i].kthrd,
			_DEVFS_THREAD_AFFNITY + i);

		wake_up_process(crfss_device_thread[process_idx][i].kthrd);
#endif //CRFS_SCHED_THREAD

#else
		/* initialize per device thread rd list */
		crfss_device_thread[i].dummy_rd.state = DEVFS_RD_BUSY;
		crfss_device_thread[i].dummy_rd.closed = 0;
		INIT_LIST_HEAD(&crfss_device_thread[i].dummy_rd.list);

		/* initialize state */
		crfss_device_thread[i].state = DEV_THREAD_IDLE;

		/* initialize mutex */
		crfss_mutex_init(&crfss_device_thread[i].per_thread_mutex);

		/* initialize current serving queue */
		crfss_device_thread[i].current_rd_list = NULL;

		/* initialize number of active file pointers */
		crfss_device_thread[i].rd_nr = 0;

#ifndef CRFS_SCHED_THREAD
		/* create kthread to simulate device thread */
		crfss_device_thread[i].kthrd = kthread_create(crfss_io_scheduler,
						&crfss_device_thread[i], "kdevio");

		if (IS_ERR(crfss_device_thread[i].kthrd)) {
			printk(KERN_ALERT "%s:%d Failed kthread create \n",
			__FUNCTION__,__LINE__);
			retval = -EFAULT;
			goto err_scheduler_init;
		}
		kthread_bind(crfss_device_thread[i].kthrd,
			_DEVFS_THREAD_AFFNITY + i);

		wake_up_process(crfss_device_thread[i].kthrd);
#endif //CRFS_SCHED_THREAD


#endif
	}

	/* set global scheduler init flag */
#ifdef CRFS_MULTI_PROC
	g_crfss_scheduler_init[process_idx] = 1;
#else
	g_crfss_scheduler_init = 1;
#endif

#ifndef CRFS_SCHED_THREAD
err_scheduler_init:
#endif
	return retval;
}

#ifndef _DEVFS_PER_THREAD_RD_LIST
void crfss_scheduler_exit(void) {
	int i = 0;
	struct list_head *cur = g_rd_list->next;
	struct crfss_fstruct *rd = NULL;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	crfss_dbg("kthread exit getting called...\n");

#ifdef _DEVFS_STAT
	crfss_stat_fp_queue_count();
#endif

	/* wait if an rd has pending request not done by device thread */
	while (cur && cur->next != g_rd_list) {
		rd = list_entry(cur, struct crfss_fstruct, list);
		cur = cur->next;

#ifndef CRFS_BYPASS_KERNEL
checkqueue:
		crfss_mutex_lock(&rd->read_lock);
		if (rd->fifo.head != rd->fifo.tail) {
			crfss_mutex_unlock(&rd->read_lock);
			goto checkqueue;
		}
		crfss_mutex_unlock(&rd->read_lock);
#endif
	}

#ifdef CRFS_MULTI_PROC
	/* initialize device thread structure */
	for (i = 0; i < crfss_device_thread_nr[process_idx]; ++i) {

		/* change device thread state to be EXIT */
		crfss_device_thread[process_idx][i].state = DEV_THREAD_EXIT;

#ifndef CRFS_SCHED_THREAD
		/* stop kthreads */
		kthread_stop(crfss_device_thread[process_idx][i].kthrd);
#endif
	}

	/* set global scheduler init flag */
	g_crfss_scheduler_init[process_idx] = 0;

#else
	/* initialize device thread structure */
	for (i = 0; i < crfss_device_thread_nr; ++i) {

		/* change device thread state to be EXIT */
		crfss_device_thread[i].state = DEV_THREAD_EXIT;

#ifndef CRFS_SCHED_THREAD
		/* stop kthreads */
		kthread_stop(crfss_device_thread[i].kthrd);
#endif
	}

	/* set global scheduler init flag */
	g_crfss_scheduler_init = 0;
#endif //CRFS_MULTI_PROC

	crfss_dbg("Terminating device threads...\n");
}
#else
void crfss_scheduler_exit(void) {
	int i = 0;
	struct list_head *l_rd_list = NULL, *cur = NULL;
	struct crfss_fstruct *rd = NULL;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	crfss_dbg("kthread exit getting called...\n");

#ifdef _DEVFS_STAT
	crfss_stat_fp_queue_count();
#endif

#ifdef CRFS_MULTI_PROC
	/* initialize device thread structure */
	for (i = 0; i < crfss_device_thread_nr[process_idx]; ++i) {

		l_rd_list = &crfss_device_thread[process_idx][i].dummy_rd.list;
		cur = l_rd_list->next;

		/* wait if an rd has pending request not done by device thread */
		while (cur && cur->next != l_rd_list) {
			rd = list_entry(cur, struct crfss_fstruct, list);
			cur = cur->next;

#ifndef CRFS_BYPASS_KERNEL
checkqueue:
			crfss_mutex_lock(&rd->read_lock);
			if (rd->fifo.head != rd->fifo.tail) {
				crfss_mutex_unlock(&rd->read_lock);
				goto checkqueue;
			}
			crfss_mutex_unlock(&rd->read_lock);
#endif
		}

		/* change device thread state to be EXIT */
		crfss_device_thread[process_idx][i].state = DEV_THREAD_EXIT;

#ifndef CRFS_SCHED_THREAD
		/* stop kthreads */
		kthread_stop(crfss_device_thread[process_idx][i].kthrd);
#endif
	}

	/* set global scheduler init flag */
	g_crfss_scheduler_init[process_idx] = 0;

#else
	/* initialize device thread structure */
	for (i = 0; i < crfss_device_thread_nr; ++i) {

		l_rd_list = &crfss_device_thread[i].dummy_rd.list;
		cur = l_rd_list->next;

		/* wait if an rd has pending request not done by device thread */
		while (cur && cur->next != l_rd_list) {
			rd = list_entry(cur, struct crfss_fstruct, list);
			cur = cur->next;

#ifndef CRFS_BYPASS_KERNEL
checkqueue:
			crfss_mutex_lock(&rd->read_lock);
			if (rd->fifo.head != rd->fifo.tail) {
				crfss_mutex_unlock(&rd->read_lock);
				goto checkqueue;
			}
			crfss_mutex_unlock(&rd->read_lock);
#endif
		}

		/* change device thread state to be EXIT */
		crfss_device_thread[i].state = DEV_THREAD_EXIT;

#ifndef CRFS_SCHED_THREAD
		/* stop kthreads */
		kthread_stop(crfss_device_thread[i].kthrd);
#endif
	}

	/* set global scheduler init flag */
	g_crfss_scheduler_init = 0;

#endif //CRFS_MULTI_PROC

	crfss_dbg("Terminating device threads...\n");
}

#endif


/***************************************************************
 *******************	 Scheduler Code    ********************
 **************************************************************/

/* Round-robin scheduling policy */
static struct crfss_fstruct* crfss_sched_round_robin
				(struct dev_thread_struct *dev_thread_ctx) {
	struct crfss_fstruct *rd = NULL;
	//struct crfss_fstruct *target = NULL;
	//struct list_head *cur = NULL;

	if (!dev_thread_ctx) {
		printk(KERN_ALERT "%s:%d Device thread context is NULL \n",
			__FUNCTION__,__LINE__);
		return NULL;
	}

	/* 
	 * Case 1 (Fast Path):
	 * If current serving is not NULL, picking next available rd queue
	 * until reaching the end of global rd list
	 */
	if (dev_thread_ctx->current_rd_list &&
		dev_thread_ctx->current_rd_list->next != g_rd_list) {

		rd = list_entry(dev_thread_ctx->current_rd_list,
			struct crfss_fstruct, list);

		if (!rd || IS_ERR(rd)) {
			goto rr_slow_path;
		}

#ifdef _DEVFS_SCHEDULER_RCU
		rcu_read_lock();
		list_for_each_entry_continue_rcu(rd, g_rd_list, list) {
#else
		crfss_mutex_lock(&g_mutex);		
		list_for_each_entry_continue(rd, g_rd_list, list) {
#endif
			if (!rd || IS_ERR(rd)) {
				goto rr_fast_path_end;
			}

			/* If it is a read operation, then just schedule it */
			if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
				dev_thread_ctx->current_rd_list = &rd->list;
				goto rr_found_rd;
			}
		}
rr_fast_path_end:
#ifdef _DEVFS_SCHEDULER_RCU
		rcu_read_unlock();
#else
		crfss_mutex_unlock(&g_mutex);
#endif
	}

rr_slow_path:
	/* 
	 * Case 2 (Slow Path):
	 * If current serving is NULL,
	 * walk through the global rd list from beginning
	 */
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_lock();
	list_for_each_entry_rcu(rd, g_rd_list, list) {
#else
	crfss_mutex_lock(&g_mutex);		
	list_for_each_entry(rd, g_rd_list, list) {
#endif
		if (!rd || IS_ERR(rd)) {
			rd = NULL;
			goto rr_found_rd;
		}

		/* If it is a read operation, then just schedule it */
		if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
			dev_thread_ctx->current_rd_list = &rd->list;
			goto rr_found_rd;
		}
	}
rr_found_rd:
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_unlock();
#else
	crfss_mutex_unlock(&g_mutex);
#endif
	return rd;
}


/* Read prioritized scheduling policy */
static struct crfss_fstruct* crfss_sched_read_prioritized
				(struct dev_thread_struct *dev_thread_ctx) {
	struct crfss_fstruct *rd = NULL;
	struct crfss_fstruct *target = NULL;
	//struct list_head *cur = NULL;
	nvme_cmdrw_t *cmdrw = NULL;

	unsigned long cur_tsc = jiffies;
	unsigned long wait_time = 0;
	unsigned long longest = 0;

	if (!dev_thread_ctx) {
		printk(KERN_ALERT "%s:%d Device thread context is NULL \n",
			__FUNCTION__,__LINE__);
		return NULL;
	}

	/* 
	 * Case 1 (Fast Path):
	 * If current serving is not NULL, picking next available rd queue
	 * until reaching the end of global rd list
	 */
	if (dev_thread_ctx->current_rd_list &&
		dev_thread_ctx->current_rd_list != g_rd_list) {

#ifdef _DEVFS_SCHEDULER_RCU
		rcu_read_lock();
		rd = list_entry_rcu(dev_thread_ctx->current_rd_list,
					struct crfss_fstruct, list);
		list_for_each_entry_continue_rcu(rd, g_rd_list, list) {
#else
		crfss_mutex_lock(&g_mutex);
		rd = list_entry(dev_thread_ctx->current_rd_list,
					struct crfss_fstruct, list);
		list_for_each_entry_continue(rd, g_rd_list, list) {
#endif
			if (!rd || IS_ERR(rd)) {
				continue;
			}

			/* 
			 * Check if the request is a read or write
			 * If it is a write operation that 
			 * not yet reach the deadline
			 * (jiffies - rd->TSC < Threshold)
			 * Then just continue
			 * Otherwise, issue it */

			/* Only if rd is not scheduled by other device thread */
			if (rd->state == DEVFS_RD_IDLE) {
				if (test_bit(0, &rd->closed) == 1) {
					/* This is a corner case
					 * If this rd (file pointer) is closed by host thread,
					 * then schedule it first */
					if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
						dev_thread_ctx->current_rd_list = &rd->list;
						goto rp_found_rd;
					} else
						continue;
				}

				/* Fetch command from the head of rd request queue */
				//cmdrw = rd->req;
				cmdrw = rd_queue_readtail(rd, sizeof(__u64));

				/* Checking queue empty */
				if (cmdrw == NULL) {
					continue;
				}

				/* 
				 * If it is a write(append) operation that not yet reach
				 * the deadline, then just continue, we could schedule it later
				 */
				if (cmdrw->common.opc == nvme_cmd_write ||
					cmdrw->common.opc == nvme_cmd_append ) {

					/* In case jiffie counter is overflown */
					if (time_after(cur_tsc, rd->tsc))	
						wait_time = jiffies_to_usecs(cur_tsc - rd->tsc);
					else
						wait_time = jiffies_to_usecs(ULONG_MAX - rd->tsc + cur_tsc); 

					if (wait_time < WRITE_TIMEOUT) {
						if (wait_time > longest) {
							longest = wait_time;
							target = rd;
						}
						continue;
					}
				}	

				/*
				 * Reaching here, the request is either a read request,
				 * or a write request approaching deadline.
				 * Then schedule it.
				 */
				if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
					dev_thread_ctx->current_rd_list = &rd->list;
					goto rp_found_rd;
				}
			}
		}
#ifdef _DEVFS_SCHEDULER_RCU
		rcu_read_unlock();
#else
		crfss_mutex_unlock(&g_mutex);
#endif
	}

	/*
	 * Case 2 (Slow Path):
	 * If current serving is NULL, 
	 * walk through the global rd list from beginning
	 */
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_lock();
	list_for_each_entry_rcu(rd, g_rd_list, list) {
#else
	crfss_mutex_lock(&g_mutex);
	list_for_each_entry(rd, g_rd_list, list) {
#endif
		if (!rd || IS_ERR(rd)) {
			continue;
		}

		/* 
		 * Check if the request is a read or write
		 * If it is a write operation that not yet reach the deadline
		 * (jiffies - rd->TSC < Threshold)
		 * Then just continue
		 * Otherwise, issue it */

		/* Only if rd is not scheduled by other device thread */
		if (rd->state == DEVFS_RD_IDLE) {
			if (test_bit(0, &rd->closed) == 1) {
				/* This is a corner case
				 * If this rd (file pointer) is closed by host thread,
				 * then schedule it first */
				if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
					dev_thread_ctx->current_rd_list = &rd->list;
					goto rp_found_rd;
				} else
					continue;
			}

			/* Fetch command from the head of rd request queue */
			//cmdrw = rd->req;
			cmdrw = rd_queue_readtail(rd, sizeof(__u64));

			/* Checking queue empty */
			if (cmdrw == NULL) {
				continue;
			}

			/* 
			 * If it is a write(append) operation that not yet reach the deadline
			 * Then just continue, we could schedule it later
			 */
			if (cmdrw->common.opc == nvme_cmd_write ||
				cmdrw->common.opc == nvme_cmd_append ) {
			
				/* In case jiffie counter is overflown */
				if (time_after(cur_tsc, rd->tsc))	
					wait_time = jiffies_to_usecs(cur_tsc - rd->tsc);
				else
					wait_time = jiffies_to_usecs(ULONG_MAX - rd->tsc +  cur_tsc); 

				if (wait_time < WRITE_TIMEOUT) {
					if (wait_time > longest) {
						longest = wait_time;
						target = rd;
					}
					//target = rd;
					continue;
				}
			}	

			/*
			 * Reaching here, the request is either a read request,
			 * or a write request approaching deadline.
			 * Then schedule it.
			 */
			if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
				dev_thread_ctx->current_rd_list = &rd->list;
				goto rp_found_rd;
			}
		}
	}
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_unlock();
#else
	crfss_mutex_unlock(&g_mutex);
#endif

	/* If there is no read request, pick a write request */
	if (target && test_and_set_bit(0, &target->state) == DEVFS_RD_IDLE) {
		dev_thread_ctx->current_rd_list = &target->list;
		return target;
	}
	return NULL;

rp_found_rd:
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_unlock();
#else
	crfss_mutex_unlock(&g_mutex);
#endif
	return rd;
}


/* Per thread rd list scheduling policy */
static struct crfss_fstruct* crfss_sched_per_thread_rd_list
				(struct dev_thread_struct *dev_thread_ctx) {
	struct crfss_fstruct *rd = NULL;
	//struct crfss_fstruct *target = NULL;
	struct list_head *l_rd_list = &dev_thread_ctx->dummy_rd.list;

#ifndef CRFS_PERF_TUNING
	if (!dev_thread_ctx) {
		printk(KERN_ALERT "%s:%d Device thread context is NULL \n",
			__FUNCTION__,__LINE__);
		return NULL;
	}
#endif

	/* 
	 * Case 1 (Fast Path):
	 * If current serving is not NULL, picking next available rd queue
	 * until reaching the end of global rd list
	 */
#ifndef CRFS_PERF_TUNING
	if (dev_thread_ctx->current_rd_list &&
		dev_thread_ctx->current_rd_list->next != l_rd_list) {

		rd = list_entry(dev_thread_ctx->current_rd_list,
			struct crfss_fstruct, list);

		if (!rd || IS_ERR(rd)) {
			goto per_thread_rd_list_slow_path;
		}
#else
	if (dev_thread_ctx->current_rd_list) {
		
		rd = dev_thread_ctx->current_rd;
#endif

#ifdef _DEVFS_SCHEDULER_RCU
		rcu_read_lock();
		list_for_each_entry_continue_rcu(rd, l_rd_list, list) {
#else
		crfss_mutex_lock(&dev_thread_ctx->per_thread_mutex);		
		list_for_each_entry_continue(rd, l_rd_list, list) {
#endif
			if (!rd || IS_ERR(rd)) {
				goto per_thread_rd_list_fast_path_end;
			}

			//printk(KERN_ALERT "get next rd = %llx\n", rd);
			/* If it is a read operation, then just schedule it */
			if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
				dev_thread_ctx->current_rd_list = &rd->list;
#ifdef CRFS_PERF_TUNING
				dev_thread_ctx->current_rd = rd;
#endif
				goto per_thread_rd_list_found_rd;
			}
		}
per_thread_rd_list_fast_path_end:
#ifdef _DEVFS_SCHEDULER_RCU
		rcu_read_unlock();
#else
		crfss_mutex_unlock(&dev_thread_ctx->per_thread_mutex);
#endif
	}

per_thread_rd_list_slow_path:
	/* 
	 * Case 2 (Slow Path):
	 * If current serving is NULL,
	 * walk through the global rd list from beginning
	 */
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_lock();
	list_for_each_entry_rcu(rd, l_rd_list, list) {
#else
	crfss_mutex_lock(&dev_thread_ctx->per_thread_mutex);		
	list_for_each_entry(rd, l_rd_list, list) {
#endif
		if (!rd || IS_ERR(rd)) {
			rd = NULL;
			goto per_thread_rd_list_found_rd;
		}

		/* If it is a read operation, then just schedule it */
		if (test_and_set_bit(0, &rd->state) == DEVFS_RD_IDLE) {
			dev_thread_ctx->current_rd_list = &rd->list;
#ifdef CRFS_PERF_TUNING
			dev_thread_ctx->current_rd = rd;
#endif
			goto per_thread_rd_list_found_rd;
		}
	}

per_thread_rd_list_found_rd:
#ifdef _DEVFS_SCHEDULER_RCU
	rcu_read_unlock();
#else
	crfss_mutex_unlock(&dev_thread_ctx->per_thread_mutex);
#endif
	return rd;
}


/* Pick next file pointer queue to run */
static inline struct crfss_fstruct* crfss_sched_pick_next
			(struct dev_thread_struct *dev_thread_ctx, int policy) {


	switch (policy) {
		case DEVFS_SCHED_ROUND_ROBIN:
			return crfss_sched_round_robin(dev_thread_ctx);

		case DEVFS_SCHED_READ_PRIO:
			return crfss_sched_read_prioritized(dev_thread_ctx);

		case DEVFS_SCHED_PER_THREAD_RD_LIST:
			return crfss_sched_per_thread_rd_list(dev_thread_ctx);

		default:
			return crfss_sched_round_robin(dev_thread_ctx);
	}
}


/* Function assumes that the caller has already checked if rd != NULL */
inline int process_request(struct crfss_fstruct *rd) 
{

	nvme_cmdrw_t *cmdrw;
	struct file *fp = NULL;
	struct inode *inode = NULL;

	/* Get the inode of this rd */
	fp = rd->fp;
	if (!fp) {
		test_and_clear_bit(0, &rd->state);
		return -1;
	}
	inode = fp->f_inode;
	if (!inode) {
		test_and_clear_bit(0, &rd->state);
		return -1;
	}

	/* Get the first request in rd queue */
	cmdrw = rd_queue_readtail(rd, sizeof(__u64));

	/* Checking queue empty */
	if (cmdrw == NULL) {
		test_and_clear_bit(0, &rd->state);
		return -1;
	}

	/* Set credential */
	crfss_set_cred(rd);

#ifdef _DEVFS_FSYNC_ENABLE
	/*
	 * fsync barrier is a special case.
	 * When an fsync barrier flag is set, instead of process one request
	 * this round, flush all the write/append operations issued prior to the
	 * fsync
	 */
	if (test_bit(0, (const volatile long unsigned int *)&inode->fsync_barrier) == 1 &&
	    atomic_read(&inode->fsync_counter) > 0 &&
	    cmdrw->common.opc != nvme_cmd_flush) {
		vfio_crfss_io_fsync(rd);
		goto finish_process_request;
	}
#endif

	if (cmdrw->common.opc == nvme_cmd_read) {
		/* Process read request */
		vfio_process_read(rd, cmdrw);

	} else if (cmdrw->common.opc == nvme_cmd_write) {
		/* Process write request */
		vfio_process_write(rd, cmdrw);

	} else if ( cmdrw->common.opc == nvme_cmd_append ) {
		/* Process append request */
		vfio_process_append(rd, cmdrw);

	} else if ( cmdrw->common.opc == nvme_cmd_flush ) { 
		/* Process fsync request */
		vfio_process_fsync(rd, cmdrw, inode);

	} else if ( cmdrw->common.opc == nvme_cmd_close ) {
		/* Process close request */
		vfio_process_close(rd, cmdrw);

	} else if ( cmdrw->common.opc == nvme_cmd_unlink ) {
		/* Process unlink request */
		vfio_process_unlink(rd, cmdrw);

	} else if ( cmdrw->common.opc == nvme_cmd_lseek ) {
		// TODO
	} else {
		// TODO
	}

finish_process_request:
	test_and_clear_bit(0, &rd->state);

	return 0;
}

#ifdef CRFS_THRDLIST_ARR
int global_id=0;
//XXXX
void initialize_thrd_rdlist(struct dev_thread_struct *dev_thread_ctx){

	int i=0;
	for(i=0; i< MAX_RDS; i++)
		dev_thread_ctx->rd_list_array[i] = NULL;

	dev_thread_ctx->nxt_free_slot = 0;
	dev_thread_ctx->prev_rd = -1;
	dev_thread_ctx->id = global_id++;
}

struct crfss_fstruct debug_rd_list(struct dev_thread_struct *thread)
{
	int i = 0;

	printk("******START****%d\n", thread->id);
	printk("valid %d: ", i);

        for(i =0; i < MAX_RDS; i++)
		if(thread->rd_list_array[i] != NULL) 
			printk("%d, ", i);

	 printk("******END****%d\n", thread->id);
}

int add_to_thrd_rdlist(struct crfss_fstruct *rd, struct dev_thread_struct *thread) 
{

	int idx = thread->nxt_free_slot;
	int cnt = 0;

	if(thread->rd_list_array[idx] == NULL) {
		thread->rd_list_array[idx] = rd;
		rd->dev_thread_slot = idx;	
		//debug_rd_list(thread);
		return 0;
	}

	for(cnt = 0; cnt < MAX_RDS; cnt++) {
		if(thread->rd_list_array[cnt] == NULL) {
			thread->rd_list_array[cnt] = rd;
			rd->dev_thread_slot = cnt;
			//debug_rd_list(thread);
			return 0;
		}
	}
list_add_err:
	printk(KERN_ALERT "Could not find anything \n");
	return -1;

list_add_success:
	return 0;
}

int del_from_thrd_rdlist(struct crfss_fstruct *rd, struct dev_thread_struct *thread)
{
	int idx = rd->dev_thread_slot;
	thread->rd_list_array[idx] = NULL;
	thread->nxt_free_slot = idx;
}



struct crfss_fstruct *pick_thrd_next_rd(struct dev_thread_struct *thread) 
{
	struct crfss_fstruct *rd = NULL;
	int next_rd = (thread->prev_rd + 1) % MAX_RDS;
	int cnt = 0, index = 0;

	if(!thread->rd_nr)
		return NULL;
#if 0
	if(thread->rd_list_array[next_rd] != NULL) {
		thread->prev_rd = next_rd;
		thread->rd_list_hits++;
		return thread->rd_list_array[next_rd];
	}
#endif
#if 1
	for(cnt = next_rd; cnt < MAX_RDS + next_rd; cnt++) {
		index = cnt % MAX_RDS;
		rd = thread->rd_list_array[index];  
		if(rd != NULL) {
			thread->prev_rd = index;
			thread->rd_list_hits++;
			return rd;
		}
	}
	thread->rd_list_miss++;
	//debug_rd_list(thread);
	//if (thread->prev_rd >= MAX_RDS -1)
	//	thread->prev_rd = -1;
#endif

pick_thrd_err:
	//printk(KERN_ALERT "Could not find anything \n");
	return NULL;

pick_thrd_success:
	return NULL;
}

// In-device kthread I/O handler
int crfss_io_scheduler(void *data) {
	struct dev_thread_struct *dev_thread_ctx = data;
	struct crfss_fstruct *rd = NULL;
	void *ptr = NULL;
	nvme_cmdrw_t *cmdrw = NULL;
	int retval = 0;

#ifdef CRFS_THRDLIST_ARR
	//XXXX
	initialize_thrd_rdlist(dev_thread_ctx);
#endif


#ifndef CRFS_SCHED_THREAD
	while (!kthread_should_stop()) {
#else
	while (dev_thread_ctx->state != DEV_THREAD_EXIT) {
#endif
		if(!dev_thread_ctx->rd_nr) {
			goto resume_spin;
		}

#ifdef CRFS_THRDLIST_ARR
		rd = pick_thrd_next_rd(dev_thread_ctx);
		if(!rd) 
#else
                /* Now picking next rd queue to schedule */
#ifndef CRFS_PERF_TUNING
                rd = crfss_sched_pick_next(dev_thread_ctx, crfss_scheduler_policy);
#else
                rd = crfss_sched_per_thread_rd_list(dev_thread_ctx);
#endif

#endif
                if (!rd || IS_ERR(rd)) 
                        goto resume_spin;

                /* Returns 0 if no error */
                if (process_request(rd)) {
                         goto resume_spin;
                }    
#ifdef CRFS_SCHED_THREAD
                dev_thread_ctx->rqsts++;
#endif
                continue;

resume_spin:
                /* Avoid hardlock after spinning 10s in kernel space */
                cond_resched();
                continue;
	}
	printk(KERN_ALERT "CRFS_THRDLIST_ARR hits %d miss %d\n", 
			dev_thread_ctx->rd_list_hits, dev_thread_ctx->rd_list_miss);

kthrd_out:
	return retval; 
}
EXPORT_SYMBOL(crfss_io_scheduler);

#else //CRFS_THRDLIST_ARR

// In-device kthread I/O handler
int crfss_io_scheduler(void *data) {
	struct dev_thread_struct *dev_thread_ctx = data;
	struct crfss_fstruct *rd = NULL;
	int retval = 0;

#ifndef CRFS_SCHED_THREAD
	while (!kthread_should_stop()) {
#else
	while (dev_thread_ctx->state != DEV_THREAD_EXIT) {
#endif
                /* Now picking next rd queue to schedule */
#ifndef CRFS_PERF_TUNING
                rd = crfss_sched_pick_next(dev_thread_ctx, crfss_scheduler_policy);
#else
                rd = crfss_sched_per_thread_rd_list(dev_thread_ctx);
#endif
                if (!rd || IS_ERR(rd)) 
                        goto resume_spin;

                /* Returns 0 if no error */
                if(process_request(rd)) {
                         goto resume_spin;
                }    
#ifdef CRFS_PERF_TUNING
                dev_thread_ctx->rqsts++;
#endif
                continue;

resume_spin:
                /* Avoid hardlock after spinning 10s in kernel space */
                cond_resched();
                continue;
	}
	return retval; 
}
EXPORT_SYMBOL(crfss_io_scheduler);

#endif //{ARAFS_THRDLIST_ARR



// Add an rd to global rd list
static int crfss_add_global_list(struct crfss_fstruct *rd) {
	int retval = 0;
	crfss_mutex_lock(&g_mutex);
#ifdef _DEVFS_SCHEDULER_RCU
	list_add_rcu(&rd->list, g_rd_list);
#else
	list_add(&rd->list, g_rd_list);
#endif
	/* Check if this rd is added successfully or not */
	if (g_rd_list->next != &rd->list) {
		printk(KERN_ALERT "DEBUG: insert per-fp queue failed | %s:%d",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;             
		crfss_mutex_unlock(&g_mutex);
		goto err_add_global_list;
	}
	crfss_mutex_unlock(&g_mutex);

err_add_global_list:
	return retval;
}

// Remove an rd to global rd list
static int crfss_del_global_list(struct crfss_fstruct *rd) {
	int retval = 0;
	crfss_mutex_lock(&g_mutex);
#ifdef _DEVFS_SCHEDULER_RCU
	list_del_rcu(&rd->list);
#else
	struct list_head *next = rd->list.next;
	list_del(&rd->list);
	rd->list.next = next;
#endif
	crfss_mutex_unlock(&g_mutex);

	return retval;
}

#ifdef CRFS_PERF_TUNING
void debug_rdper_thread(void)
{
	struct dev_thread_struct *target_thread;
	int index = 0;

#ifdef CRFS_MULTI_PROC
	for (index = 0; index < crfss_device_thread_nr[process_idx]; index++){
		target_thread = &crfss_device_thread[process_idx][index];
#else
	for (index = 0; index < crfss_device_thread_nr; index++){
		target_thread = &crfss_device_thread[index];
#endif
		printk(KERN_ALERT "Thrd[%d]: %d Rqsts processed %d\n", 
			index+1, target_thread->rd_nr,target_thread->rqsts);
	}
}

// Add an rd to per device thread rd list
static int crfss_add_per_thread_list(struct crfss_fstruct *rd) {
	int retval = 0, index = 0, min = INT_MAX, i = 0;
	struct list_head *target_list;
	struct dev_thread_struct *target_thread;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif
	// Select a target thread in a RR manner

#ifndef CRFS_PERF_TUNING
	crfss_mutex_lock(&g_mutex);     
#endif
	//index = g_dev_thread_round;
	//g_dev_thread_round = (g_dev_thread_round + 1) % crfss_device_thread_nr;

#ifdef CRFS_MULTI_PROC
	for (i = 0; i < crfss_device_thread_nr[process_idx]; ++i) {
		if (crfss_device_thread[process_idx][i].rqsts < min) {
			index = i;
			min = crfss_device_thread[process_idx][i].rqsts;
		}
	}

#ifndef CRFS_PERF_TUNING
	crfss_mutex_unlock(&g_mutex);
#endif
	target_thread = &crfss_device_thread[process_idx][index];

#else
	for (i = 0; i < crfss_device_thread_nr; ++i) {
		if (crfss_device_thread[i].rqsts < min) {
			index = i;
			min = crfss_device_thread[i].rqsts;
		}
	}

#ifndef CRFS_PERF_TUNING
	crfss_mutex_unlock(&g_mutex);   
#endif
	target_thread = &crfss_device_thread[index]; 
#endif	//CRFS_PERF_TUNING

	target_list = &target_thread->dummy_rd.list;
	// Add selected device thread info to rd
	rd->dev_thread_ctx = target_thread;

	crfss_mutex_lock(&target_thread->per_thread_mutex);     

#ifdef CRFS_THRDLIST_ARR
	add_to_thrd_rdlist(rd, target_thread);
#else

#ifdef _DEVFS_SCHEDULER_RCU
	list_add_rcu(&rd->list, target_list);
#else 
	list_add(&rd->list, target_list);
#endif //_DEVFS_SCHEDULER_RCU

	if (target_list->next != &rd->list) {
		printk(KERN_ALERT "DEBUG: insert per-thread rd list failed | %s:%d",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;    
		crfss_mutex_unlock(&target_thread->per_thread_mutex);   
		goto err_add_per_thread_list;
	}
#endif //CRFS_THRDLIST_ARR

	++target_thread->rd_nr;
	crfss_mutex_unlock(&target_thread->per_thread_mutex);   

err_add_per_thread_list:
	return retval;
}
             

#else
// Add an rd to per device thread rd list
static int crfss_add_per_thread_list(struct crfss_fstruct *rd) {
	int retval = 0, index = 0;
	//int min = INT_MAX, i = 0;
	struct list_head *target_list;
	struct dev_thread_struct *target_thread;

#ifdef CRFS_MULTI_PROC
	int process_idx = current->tgid & (HOST_PROCESS_MAX - 1);
#endif

	// Select a target thread in a RR manner
	crfss_mutex_lock(&g_mutex);	
	index = g_dev_thread_round;
#ifdef CRFS_MULTI_PROC
	g_dev_thread_round = (g_dev_thread_round + 1) % crfss_device_thread_nr[process_idx];
#else
	g_dev_thread_round = (g_dev_thread_round + 1) % crfss_device_thread_nr;
#endif

	/*for (i = 0; i < crfss_device_thread_nr; ++i) {
		if (crfss_device_thread[i].rd_nr < min) {
			index = i;
			min = crfss_device_thread[i].rd_nr;
		}
	}*/

	crfss_mutex_unlock(&g_mutex);	

#ifdef CRFS_MULTI_PROC
	target_thread = &crfss_device_thread[process_idx][index]; 
#else
	target_thread = &crfss_device_thread[index]; 
#endif
	target_list = &target_thread->dummy_rd.list;

	// Add selected device thread info to rd
	rd->dev_thread_ctx = target_thread;

	crfss_mutex_lock(&target_thread->per_thread_mutex);	

#ifdef _DEVFS_SCHEDULER_RCU
	list_add_rcu(&rd->list, target_list);
#else
	list_add(&rd->list, target_list);
#endif

	if (target_list->next != &rd->list) {
		printk(KERN_ALERT "DEBUG: insert per-thread rd list failed | %s:%d",
			__FUNCTION__, __LINE__);
		retval = -EFAULT;    
		crfss_mutex_unlock(&target_thread->per_thread_mutex);	
		goto err_add_per_thread_list;
	}
	++target_thread->rd_nr;
	crfss_mutex_unlock(&target_thread->per_thread_mutex);	

err_add_per_thread_list:
	return retval;
}
#endif

// Remove an rd to per device thread rd list
static int crfss_del_per_thread_list(struct crfss_fstruct *rd) {
	int retval = 0;
	struct dev_thread_struct *target_thread;

	// Get target device thread from rd
	target_thread = rd->dev_thread_ctx;

	crfss_mutex_lock(&target_thread->per_thread_mutex);	

#ifdef CRFS_THRDLIST_ARR
        del_from_thrd_rdlist(rd, target_thread);
#else //CRFS_THRDLIST_ARR

#ifdef _DEVFS_SCHEDULER_RCU
	list_del_rcu(&rd->list);
#else
	struct list_head *next = rd->list.next;
	list_del(&rd->list);
	rd->list.next = next;
#endif

#endif //CRFS_THRDLIST_ARR

	--target_thread->rd_nr;
	crfss_mutex_unlock(&target_thread->per_thread_mutex);	

	return retval;
}

int crfss_scheduler_add_list(struct crfss_fstruct *rd) {
#ifndef _DEVFS_PER_THREAD_RD_LIST
	return crfss_add_global_list(rd);
#else
	return crfss_add_per_thread_list(rd);
#endif
}

int crfss_scheduler_del_list(struct crfss_fstruct *rd) {
#ifndef _DEVFS_PER_THREAD_RD_LIST
	return crfss_del_global_list(rd);
#else
	return crfss_del_per_thread_list(rd);
#endif
}
