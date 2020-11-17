#define _GNU_SOURCE
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <linux/pci.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <linux/types.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>

#include "unvme_nvme.h"
#include "vfio.h"
#include "crfslib.h"
#include "crfslibio.h"
#include "crfs_client.h"

/*TODO: Cleanup Global variables */
static void* vsq;
static size_t g_vsqlen;
/* Flag set by --verbose. */
static int verbose_flag;
static int isread = 0;
static int isdelete = 0;
static int iskernelio = 0;
static int doIO = 1;
static int iosize;
static int numfs = 1;
static int waittime = 0;
char fname[NAME_MAX];
struct timeval iostart, ioend;
struct timeval fcreatstrt, fcreatend;
static double g_avgthput;
static double g_avgrthput;
static double g_avgwthput;
static unsigned int g_numops;
int getargs(int argc, char **argv);
unsigned long str_to_opscnt(char* str);

static int numreader = 0;
static int numwriter = 0;

static int fsyncfreq = 0;
static char filesize[NAME_MAX];
static unsigned long opscnt = 0;

int g_dev = 0;

pthread_t *w_t;
pthread_t *r_t;
int *threadw_idx;
int *threadr_idx;

pthread_mutex_t r_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t w_mutex = PTHREAD_MUTEX_INITIALIZER;

/* To calculate simulation time */
long simulation_time(struct timeval start, struct timeval end ) {
	long current_time;
	current_time = ((end.tv_sec*1000000 + end.tv_usec) -
					(start.tv_sec*1000000 + start.tv_usec));
	return current_time;
}

void* do_rand_read(void* arg) {
	int fd = crfsopen((char *)fname, READIR, MODE);
	printf("fd = %d\n", fd);
	if (fd < 0) {
		fprintf(stderr,"crfsopen failed %d \n", errno);
		exit(1);
	}

	u64 datasize = iosize;    
	u64 slba = 0;
	u64 ops = opscnt; 
	u64 nlb = 0;
	u64 pat, w;
	int q = 0;

	/* Stats */
	u64 ts = 0, rtdcpersec = 0, tsc = 0;
	double sec = 0.0;
	double thruput=0;
	struct timeval start, end;

	char *p = malloc(datasize);
	if (!p) {
		printf("user buffer malloc failed\n");
		return NULL;
	}
	memset(p, 0, datasize);

	//pat = time(0);
	/* Start timer */
	//tsc = rdtsc();
	gettimeofday(&start, NULL);

	/* perform IO write */    
	for (w = 0; w < ops; w++) {
		if (slba >= MAX_SLBA)
			break;

		nlb = datasize;
		slba = (rand() % ops) * datasize;

		//printf("clear data: %lx | %s\n", p, p);

#ifdef _USE_DEVFS_CALLS	
		if(crfspread(fd, p, nlb, slba) != nlb){
#else
		if(pread(fd, p, nlb, slba) != nlb){
#endif
			printf("crfsread failed \n");
			return NULL;	
		}


#ifdef _VERIFY_DATA
		if (p[0] != 'a' + (slba / datasize) % 26) {
			printf("Content mismatch\n");
			printf("Supposed to be %c, but get %c\n",
				'a' + (slba / datasize) % 26, p[0]);
			break;
		}
#endif

		//printf("get data: %c\n", p[0]);
	}
    
	//ts = rdtsc_elapse((u64)(tsc));
	//rtdcpersec = rdtsc_second();
	//sec = (double)ts/(double)rtdcpersec;
	gettimeofday(&end, NULL);
	sec = simulation_time(start, end);
	thruput = (double)(w * iosize)/(double)sec;

	pthread_mutex_lock(&r_mutex);
	g_avgrthput += thruput;	
	pthread_mutex_unlock(&r_mutex);

	g_numops = w; 

	free(p);

	crfsclose(fd);
	return NULL;
}

void* do_rand_write(void* arg) {
	int fd = crfsopen((char *)fname, READIR, MODE);
	printf("fd = %d\n", fd);
	if (fd < 0) {
		fprintf(stderr,"crfsopen failed %d \n", errno);
		exit(1);
	}

	u64 datasize = iosize;    
	u64 slba = 0;
	u64 ops = opscnt; 
	u64 nlb = datasize / BLOCKSIZE;
	u64 pat, w;
	int q = 0;
	int thread_id = *(int*)arg;

	/* Stats */
	u64 ts = 0, rtdcpersec = 0, tsc = 0;
	double sec = 0.0;
	double thruput=0;
	struct timeval start, end;

	char *p = malloc(datasize);
	if (!p) {
		printf("user buffer malloc failed\n");
		return NULL;
	}
	memset(p, 0, datasize);


	//pat = time(0);
	/* Start timer */
	//tsc = rdtsc();
	gettimeofday(&start, NULL);

	/* perform IO write */    
	for (w = 0; w < ops; w++) {
		if (slba >= MAX_SLBA)
			break;

		nlb = datasize;
		slba = (rand() % ops) * iosize;

		memset(p, 'a'+ slba % 26, iosize);

		//printf("clear data: %lx | %s\n", p, p);

#ifdef _USE_DEVFS_CALLS
		if(crfspwrite(fd, p, nlb, slba) != nlb){
#else
                if(pwrite(fd, p, nlb, slba) != nlb){
#endif
			printf("crfswrite failed \n");
			return NULL;	
		}

		//if (thread_id == 0 && fsyncfreq > 0 && w % fsyncfreq == 0)
		if (fsyncfreq > 0 && w % fsyncfreq == 0)
#ifdef _USE_DEVFS_CALLS
			crfsfsync(fd);
#else
			fsync(fd);
#endif

		//printf("get data: %s\n", p);
	}
    
	//ts = rdtsc_elapse((u64)(tsc));
	//rtdcpersec = rdtsc_second();
	//sec = (double)ts/(double)rtdcpersec;
	gettimeofday(&end, NULL);
	sec = simulation_time(start, end);
	thruput = (double)(w * iosize)/(double)sec;
	
	pthread_mutex_lock(&w_mutex);
	g_avgwthput += thruput;	
	pthread_mutex_unlock(&w_mutex);

	g_numops = w; 

	free(p);

	crfsclose(fd);
	return NULL;
}

void* do_seq_write(void* arg) {
	int fd = crfsopen((char *)fname, CREATDIR, MODE);
	printf("fd = %d\n", fd);
	if (fd < 0) {
		fprintf(stderr,"crfsopen failed %d \n", errno);
		exit(1);
	}

	u64 datasize = iosize;    
	u64 slba = 0;
	u64 ops = opscnt; 
	u64 nlb = datasize / BLOCKSIZE;
	u64 pat, w;
	int q = 0;

	/* Stats */
	u64 ts = 0, rtdcpersec = 0, tsc = 0;
	double sec = 0.0;
	double thruput=0;
	struct timeval start, end;

	char *p = malloc(datasize);
	if (!p) {
		printf("user buffer malloc failed\n");
		return NULL;
	}
	memset(p, 0, datasize);


	//pat = time(0);
	/* Start timer */
	//tsc = rdtsc();
	gettimeofday(&start, NULL);

	/* perform IO write */    
	for (w = 0; w < ops; w++) {
		if (slba >= MAX_SLBA)
			break;

		nlb = datasize;
		slba = (rand() % ops) * iosize;

		memset(p, 'a'+ slba % 26, iosize);

		//printf("clear data: %lx | %s\n", p, p);

#ifdef _USE_DEVFS_CALLS
                if(crfswrite(fd, p, nlb) != nlb){
#else
                if(write(fd, p, nlb) != nlb){
#endif
			printf("crfswrite failed \n");
			return NULL;	
		}

		if (fsyncfreq > 0 && w % fsyncfreq == 0)
#ifdef _USE_DEVFS_CALLS
			fsync(fd);
#else			
			crfsfsync(fd);
#endif

		//printf("get data: %s\n", p);
	}
    
	//ts = rdtsc_elapse((u64)(tsc));
	//rtdcpersec = rdtsc_second();
	//sec = (double)ts/(double)rtdcpersec;
	gettimeofday(&end, NULL);
	sec = simulation_time(start, end);
	thruput = (double)(w * iosize)/(double)sec;
	
	pthread_mutex_lock(&w_mutex);
	g_avgwthput += thruput;	
	pthread_mutex_unlock(&w_mutex);

	g_numops = w; 

	free(p);

	crfsclose(fd);
	return NULL;
}


/* Main function to read options and initiate 
 * benchmarking
 */
int main(int argc, char *argv[]) {
	u64 datasize;
	void* wrbuf = NULL;
	u64* p = NULL;    
	int q = 0, fd = -1, dev = -1;
	int perm = CREATDIR, idx = 0;
	int cmd = nvme_cmd_append;
	char buffer[NAME_MAX];
	double sec = 0.0;    
	int len =NAME_MAX;	
	int i = 0;

	getargs(argc, argv);

	if (argc < 2) {
		printf("Incorrect number of arguments \n");
		exit(-1);
	}

	if (argc > 2) {
		if (isread) {
			perm = READIR;
			cmd = nvme_cmd_read;    
			isjourn    = 0;
		}
	}
	datasize = iosize;
	wrbuf = malloc(datasize);
   
	if (!wrbuf) { 
		//error(1, 0, "unvme_alloc %ld failed", datasize);
		printf("wrbuf malloc failed!\n");
		exit(1);
	}

	memset(wrbuf,'a', datasize);    
	p = wrbuf;

	opscnt = str_to_opscnt(filesize);	

	crfsinit(USE_DEFAULT_PARAM, USE_DEFAULT_PARAM, USE_DEFAULT_PARAM);

#if !defined(_POSIXIO)
	dev = open(TEST, CREATDIR, MODE);
	if (dev == -1){
		printf("Error!");   
		exit(1);             
	}
#else
	dev = open(TEST, perm, MODE);
	if (dev == -1){
		printf("Error!");   
		exit(1);             
	}
#endif

	gettimeofday(&fcreatstrt, NULL);

	for (idx = 0; idx < numfs; idx++) {

		memset(buffer, 0, len);
		len = strlen(fname);    
		memcpy(buffer, fname, len);
		/*strcat(buffer, "_");
		snprintf(buffer+strlen(buffer), 8, "%d", idx);*/
		len = strlen(buffer);
		buffer[len] = 0;

		if (numreader < 0 || numwriter < 0) {
			if (numreader == -1 && numwriter == -1) {
				do_seq_write(NULL);
			} else {
				if (isread)
					do_rand_read(NULL);
				else
					do_rand_write(NULL);
			}	

		} else {
			g_dev = dev;

			w_t = malloc(numwriter*sizeof(pthread_t));
			r_t = malloc(numreader*sizeof(pthread_t));
			threadw_idx = malloc(numwriter*sizeof(int));
			threadr_idx = malloc(numreader*sizeof(int));


			for (i = 0; i < numwriter; ++i) {
				threadw_idx[i] = i;
				pthread_create(&w_t[i], NULL, &do_rand_write, &threadw_idx[i]);
			}

			for (i = 0; i < numreader; ++i) {
				threadr_idx[i] = i;
				pthread_create(&r_t[i], NULL, &do_rand_read, &threadr_idx[i]);
			}

			for (i = 0; i < numwriter; ++i)
				pthread_join(w_t[i], NULL);

			for (i = 0; i < numreader; ++i)
				pthread_join(r_t[i], NULL);

			free(w_t);
			free(r_t);
			free(threadw_idx);
			free(threadr_idx);
		}

		if (isdelete) {
			remove(buffer);
		} 
	}

	gettimeofday(&fcreatend, NULL);
	sec = simulation_time(fcreatstrt, fcreatend);
	sec = sec/1000000;

	if (numreader < 0 || numwriter < 0) {
		if (numreader == -1 && numwriter == -1) {
			fprintf(stderr,"avg thruput %lf  sec %lf \n", g_avgthput/numfs, sec);
		} else {
			if (!isread)
				fprintf(stderr, "Write avg thruput %lf  sec %lf \n", g_avgwthput, sec);
			else 
				fprintf(stderr, "Read avg thruput %lf  sec %lf \n", g_avgrthput, sec);
		}
	} else {
		fprintf(stderr,"aggregated reader thruput %lf  sec %lf \n", g_avgrthput, sec);
		fprintf(stderr,"aggregated writer thruput %lf  sec %lf \n", g_avgwthput, sec);
	}

	printf("ops/sec %lf sec %lf \n", (float)((numfs * g_numops)/sec), sec);

	crfsexit();

	if (waittime)
		while(1);

	close(dev);
	return 0;
}


int getargs(int argc, char **argv) {
	int c;

	while (1) {
		static struct option long_options[] = {
			{"file",    required_argument, 0, 'f'},
			{"isjourn",  required_argument,0, 'j'},
			{"iskernelio", required_argument,0, 'k'},
			{"filecreate",  required_argument, 0, 'g'},
			{"deletefile",  required_argument, 0, 'd'},
			{"qentrycount", required_argument, 0, 'q'},
			{"waitexit", required_argument, 0, 'w'},
			{"isread", required_argument,      0, 'r'},
			{"doIO", required_argument,      0, 'i'},
			{"iosize", required_argument,      0, 's'},
			{"reader", required_argument,      0, 't'},
			{"writer", required_argument,      0, 'u'},
			{"schedpolicy", required_argument, 0, 'p'},
			{"devcorecnt", required_argument,  0, 'v'},
			{"fsyncfreq", required_argument,  0, 'a'},
			{"filesize", required_argument,  0, 'b'},
			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long (argc, argv, "f:j:k:r:g:d:q:w:i:s:t:u:p:v:a:b:",
						long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
				break;
				//printf ("option %s", long_options[option_index].name);
				if (optarg)
					printf (" with arg %s", optarg);
					printf ("\n");
				break;

			case 'i':
				doIO = atoi(optarg);     
				break;

			case 'k':
				iskernelio = atoi(optarg);     
				break;
	
			case 's':
				iosize = atoi(optarg);    
				break;

			case 'j':
				isjourn = atoi(optarg);
				break;

			case 'r':
				isread = 1;
				break;

			case 'd':
				isdelete = atoi(optarg);
				break;

			case 'g':
				numfs = atoi(optarg);
				break;

			case 'w':
				waittime = atoi(optarg);
				break;

			case 'q':
				qentrycount = atoi(optarg);
				break;

			case 'f':
				strcpy(fname, optarg);
				break;

			case 't':
				numreader = atoi(optarg);
				break;

			case 'u':
				numwriter = atoi(optarg);
				break;

			case 'p':
				schedpolicy = atoi(optarg);
				break;

			case 'v':
				devcorecnt = atoi(optarg);
				break;

			case 'a':
				fsyncfreq = atoi(optarg);
				break;

			case 'b':
				strcpy(filesize, optarg);
				break;

			case '?':
				/* getopt_long already printed an error message. */
				break;

			default:
				abort();
		}
	}

	/* Instead of reporting --verbose
	 * and --brief as they are encountered,
	 * we report the final status resulting from them. */
	if (verbose_flag)
	puts ("verbose flag is set");

	/* Print any remaining command line arguments (not options). */
	if (optind < argc) {
		printf ("non-option ARGV-elements: ");
		while (optind < argc)
		printf ("%s ", argv[optind++]);
		putchar ('\n');
	}

	//exit (0);
	return 0;
}

unsigned long str_to_opscnt(char* str) {
	/* magnitude is last character of size */
	char size_magnitude = str[strlen(str)-1];

	/* erase magnitude char */
	str[strlen(str)-1] = 0;

	unsigned long long file_size_bytes = strtoull(str, NULL, 0);

	switch(size_magnitude) {
		case 'g':
		case 'G':
			file_size_bytes *= 1024;
		case 'm':
		case 'M':
			file_size_bytes *= 1024;
		case '\0':
		case 'k':
		case 'K':
			file_size_bytes *= 1024;
			break;
		case 'p':
		case 'P':
			file_size_bytes *= 4;
			break;
		case 'b':
		case 'B':
			break;
		default:
			printf("incorrect size format\n");
			break;
	}
	return file_size_bytes / PAGE_SIZE;
}


