#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>

enum {MEM_SIZE = 0x400}; /* 1kB */

/* Memory map a file, and return a pointer */
/*@null@*/ /*@only@*/ 
static int *openPointer(char *filename, int write);
static void closePointer( int *base);

/* Test routines */
static void testPointer();
static void readPointer();


int main()
{
	testPointer();
        //readPointer();
	return 0;
}


/* No splint warnings for this code
 * (so long as 'only' is used in the open/close functions) */
static void readPointer(void)
{
        int *base;
        int i;

        /* Open and memory map a file */
        base = openPointer("test1", 0);
        if (base == NULL) {
                printf("NULL oops!\n");
                return;
        }
        printf("The base address is %X\n", (unsigned int)base);
        for (i = 0; i < 10; i++) {
                printf("%X\n", (unsigned int)base[i]);
        }
        closePointer(base);
}




/* No splint warnings for this code
 * (so long as 'only' is used in the open/close functions) */
static void testPointer(void)
{
	int *base;
	int i;

	/* Open and memory map a file */
	base = openPointer("test1", 1);
	if (base == NULL) {
		printf("NULL oops!\n");
		return;
	}
	printf("The base address is %X\n", (unsigned int)base);
	for (i = 0; i < 10; i++) {
		base[i] = i;
	}
	for (i = 0; i < 10; i++) {
		printf("%d\n", (unsigned int)base[i]);
	}
	closePointer(base);
}

static int *openPointer(char *filename, int write_val)
{
	int fd;
	int *base = NULL;
	char tmp[] = "X";

	if (filename == NULL) {
		return NULL;
	}

	/* Create something we can mmap */
	fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd < 0) {
		return NULL;
	}
 
       //if(write_val) { 
	(void)lseek(fd, MEM_SIZE-1, SEEK_SET);
	(void)write(fd, tmp, 1);
       //}

	/* Mmap with (PROT_READ|PROT_WRITE) to match open() flags */
        base = (int *) mmap(0, MEM_SIZE-1, PROT_READ | PROT_WRITE, MAP_PRIVATE |
                    MAP_ANON, -1, 0);

	/*base = (int *)mmap(
				NULL,
				(size_t)MEM_SIZE,
				PROT_READ|PROT_WRITE,
				MAP_SHARED,
				fd,
			    0);*/
	if(base == (int *)MAP_FAILED) {
		(void)close(fd);
		return NULL;
	}
	(void)close(fd);
	return base;
}

static void closePointer(int *base)
{
	if (base != NULL) {
		(void)munmap((caddr_t)base,(size_t)MEM_SIZE);
	}
}

