#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <dlfcn.h>

void shim_init() __attribute((constructor));
void shim_fini() __attribute((destructor));

int shim_initialized = 0;

//int (* my_printf)(const char *format, ...) = NULL;

void shim_init(void)
{
	if (!shim_initialized) {
		//asm("int $3");
		shim_initialized = 1;
	}
}

void shim_fini(void)
{
	shim_initialized = 0;
}
