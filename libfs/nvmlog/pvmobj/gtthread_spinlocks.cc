#define __USE_GNU
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sched.h>
#include <assert.h>

#include "gtthread.h"
#include "gtthread_spinlocks.h"

#define NUM_CPUS 4

// CANT EXECUTE A BUS LOCKING INSTRUCTION FROM USER MODE SO THIS IS THE BEST IMPLEMENTATION OF SPINLOCKS I COULD COME UP WITH
// HAVE NO EFFECT ON A UNIPROCESSOR SYSTEM AS WE DONT NEED LOCKING THERE

extern int gt_spinlock_init(struct gt_spinlock_t* spinlock) {
	if(!spinlock)
		return -1;
	spinlock->locked = 0;
	return 0;
}

/* spinlock function with backoff */
void gt_actual_spinlock(volatile int * spinlock)
{
        int sp_val = 1;

        while(sp_val)
        {
                __asm__ __volatile__ ("pause\n");
                if(!*((volatile int *)spinlock))
                {
                __asm__ __volatile__ (
                        "movl $0x01, %%eax\n"
                        "xchg %%eax, (%1)\n"
                        "movl %%eax, %0\n"
                        :"=m"(sp_val) /* 0 */
                        : "r"(spinlock) /* 1 */
                        : "%eax"); /* No modified flags or clobbered registers */
                }
        }
        return;
}

extern int gt_spin_lock(struct gt_spinlock_t* spinlock) {

	if(!spinlock)
		return -1;
	if(NUM_CPUS > 1){
		gt_actual_spinlock(&(spinlock->locked));
#ifdef _DEBUG
        printf("acquiring spin lock %d \n",spinlock->locked);     
#endif
	}
	return 0;	
}

extern int gt_spin_unlock(struct gt_spinlock_t *spinlock) {

	if(!spinlock)
		return -1;
	
	if(spinlock->locked) {
		spinlock->locked = 0;
#ifdef _DEBUG
		printf("releasing spin lock %d \n",spinlock->locked);
#endif
    }
	   
	return 0;
}	
