#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define __NR_nvpoolcreate      315

unsigned long  malloc_init_pool( unsigned long num_pages  ){
     return (unsigned long) syscall(__NR_nvpoolcreate, num_pages);
	 fprintf(stdout,"created pool \n");
}

int main(int argc, char *argv[]){
	
    if(malloc_init_pool(atoi(argv[1])) ==  0) {
		printf("Pool creation succeeded \n");
	}else {
		printf("Pool creation failed \n");
	}
	return 0;
}

