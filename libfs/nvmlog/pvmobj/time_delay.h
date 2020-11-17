#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <assert.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif


#define PAGE_SIZE      4096
#define CACHELINE_SIZE 64

#define CPUFREQ 2800LLU /* GHz */

#define gethrtime asm_rdtsc
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define NS2CYCLE(__ns) ((__ns) * CPUFREQ / 1000)
#define CYCLE2NS(__cycles) ((__cycles) * 1000 / CPUFREQ)
#define USEC(val) (val.tv_sec*1000000LLU + val.tv_usec)

//DRAM and NVRAM bandwidth in MB/sec
#define DRAM_BW	  2000
#define NVRAM_BW  450 


typedef uint64_t hrtime_t;

static const char __whitespaces[] = "                                                                                                                                    ";
#define WHITESPACE(len) &__whitespaces[sizeof(__whitespaces) - (len) -1]

inline void asm_cpuid() {
    asm volatile( "cpuid" :::"rax", "rbx", "rcx", "rdx");
}


inline unsigned long long asm_rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

inline unsigned long long asm_rdtscp(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtscp" : "=a"(lo), "=d"(hi)::"rcx");
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


inline
int rand_int(unsigned int *seed)
{
    *seed=*seed*196314165+907633515;
    return *seed;
}


inline
void
emulate_latency_cycles(int cycles)
{
    hrtime_t start;
    hrtime_t stop;

    start = asm_rdtsc();

    do {
        /* RDTSC doesn't necessarily wait for previous instructions to complete 
	  so a serializing instruction is usually used to ensure previous 
         instructions have completed. However, in our case this is a desirable
         property since we want to overlap the latency we emulate with the
         actual latency of the emulated instruction.  *                                              */
        stop = asm_rdtsc();
    } while (stop - start < cycles);
}


/*returns dealy in ns*/
inline unsigned long calc_delay_ns(size_t datasize){

        unsigned long delay;
        double data_MB, sec;
        unsigned long nsec;

        data_MB = (double)((double)datasize/(double)pow(10,6));
        sec =(double)((double)data_MB/(double)NVRAM_BW);
        //printf("DELAY %lf(sec) %lf(MB)\n",sec, data_MB);
        delay = sec * pow(10,9);
        //fprintf(stdout,"delay %lu \n", delay);
        return delay;
}




#ifdef __cplusplus
}
#endif

