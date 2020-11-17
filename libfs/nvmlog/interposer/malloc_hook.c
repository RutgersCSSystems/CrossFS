     /* Prototypes for __malloc_hook, __free_hook */
    #include <malloc.h>
	#include "malloc_hook.h"
	#include <pthread.h> 
	#include "nv_map.h"
	#include "error_codes.h"
	#include "nv_def.h"
	#include "checkpoint.h"
	#include "gtthread.h"
	#include "gtthread_spinlocks.h"

	#define PAGESIZE 4096

	pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t free_mutex = PTHREAD_MUTEX_INITIALIZER;
	struct gt_spinlock_t spin_lock, spin_free_lock, hook_lock;

     /* Override initializing hook from the C library. */
     void (*__malloc_initialize_hook) (void) = my_init_hook;
     
     void
     my_init_hook (void)
     {
#ifdef _USE_HOTPAGE

	   struct sigaction sa;


	   memset (&sa, 0, sizeof (sa));
	   sigemptyset(&sa.sa_mask);
	   sa.sa_handler = fault_handler;
	   sa.sa_flags   = SA_SIGINFO;
	   if (sigaction(SIGSEGV, &sa, NULL) == -1)
    	   handle_error("sigaction");



       old_malloc_hook = __malloc_hook;
       old_free_hook = __free_hook;
       __malloc_hook = my_malloc_hook;
       __free_hook = my_free_hook;

		gt_spinlock_init(&spin_lock);
		gt_spinlock_init(&spin_free_lock);
		gt_spinlock_init(&hook_lock);
#endif
     }

    void *result;
	unsigned int count=0;

	
	void disable_malloc_hook(){

		gt_spin_lock(&hook_lock);
	   __malloc_hook = old_malloc_hook;
       __free_hook = old_free_hook;
		gt_spin_unlock(&hook_lock);
	}

	void enable_malloc_hook(){

		gt_spin_lock(&hook_lock);
	   __malloc_hook = my_malloc_hook;
       __free_hook = my_free_hook;
		gt_spin_unlock(&hook_lock);
	}

	 static void *
     my_malloc_hook (size_t size, const void *caller)
     {
	    void *ptr;
    	void* (*libc_malloc)(size_t);
	    struct sigaction sa;

		gt_spin_lock(&spin_lock);

		//usleep(1000);
          /* Restore all old hooks */
       __malloc_hook = old_malloc_hook;
       __free_hook = old_free_hook;

#ifdef _USE_HOTPAGE
	   //fprintf(stdout,"allocating %u\n", count++);
	  if(size < PAGESIZE) size = PAGESIZE;	
#endif

       result = memalign(PAGESIZE,size); //(char *)(((int) result + PAGESIZE-1) & ~(PAGESIZE-1));
	   assert(result);

       /* Save underlying hooks */
       old_malloc_hook = __malloc_hook;
       old_free_hook = __free_hook;

	   add_map(result,size);
       /* Restore our own hooks */
       __malloc_hook = my_malloc_hook;
       __free_hook = my_free_hook;

		gt_spin_unlock(&spin_lock);

       return result;
     }

     static void
     my_free_hook (void *ptr, const void *caller)
     {

		//pthread_mutex_lock( &free_mutex );

		gt_spin_lock(&spin_lock);

       /* Restore all old hooks */
       __malloc_hook = old_malloc_hook;
       __free_hook = old_free_hook;
       /* Call recursively */

		//if(ptr)
       //free (ptr);
	   //remove_map(ptr);	

       /* Save underlying hooks */
       old_malloc_hook = __malloc_hook;
       old_free_hook = __free_hook;
       /* printf might call free, so protect it too. */
       //printf ("freed pointer %p\n", ptr);
       /* Restore our own hooks */
       __malloc_hook = my_malloc_hook;
       __free_hook = my_free_hook;

		gt_spin_unlock(&spin_lock);

		//pthread_mutex_unlock( &free_mutex );

     }
     

