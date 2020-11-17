#ifndef MALLOC_HOOK_H_
#define MALLOC_HOOK_H_

#ifdef __cplusplus
extern "C" {
#endif
/* Prototypes for our hooks.  */
     void my_init_hook (void);
     static void *my_malloc_hook (size_t, const void *);
     static void my_free_hook (void*, const void *);
     static void *old_malloc_hook;
     static void *old_free_hook;
	 void disable_malloc_hook();
	 void enable_malloc_hook();
#ifdef __cplusplus
}
#endif

#endif
