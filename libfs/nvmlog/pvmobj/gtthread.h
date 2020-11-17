#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sched.h>

#define MUTEX_FREE -100

#define gtthread_t long

struct gtthread_mutex {
	int mutex;
	long tid_holder;
};

extern int gtthread_create(gtthread_t* thid, int fn (void*), void*arg);

extern void gtthread_init(struct timeval *timer) ;

extern void gtthread_exit(void* exit_code);

extern void gtthread_yeild();

extern long gtthread_self();

extern int gtthread_equal(long t1, long t2);

extern int gtthread_mutex_init(struct gtthread_mutex *mutex);

extern int gtthread_mutex_lock(struct gtthread_mutex *mutex);

extern int gtthread_mutex_unlock(struct gtthread_mutex *mutex);

extern int gtthread_join(long gttid, void** status);

extern void gtthread_join_all() ;

