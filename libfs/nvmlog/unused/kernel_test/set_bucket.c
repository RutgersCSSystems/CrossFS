#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>


#define __NR_NValloc 299
#define __NR_nvpoolcreate      300

unsigned long  set_num_buckets( unsigned long num_buckets  ){
      return (unsigned long) syscall(__NR_NValloc, num_buckets);
}

int main(){

        set_num_buckets(96);
        return 0;
}

