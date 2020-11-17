#ifndef gtthread_spinlocks_h
#define gtthread_spinlocks_h



#ifdef __cplusplus
extern "C" {
#endif


struct gt_spinlock_t {
	int locked;
	long tid_holder;
};

extern int gt_spinlock_init(struct gt_spinlock_t* spinlock);
extern int gt_spin_lock(struct gt_spinlock_t* spinlock);
extern int gt_spin_unlock(struct gt_spinlock_t *spinlock);

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif
