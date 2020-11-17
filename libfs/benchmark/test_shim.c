#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "unvme_nvme.h"
#include "crfslib.h"

#define TESTDIR "/mnt/ram"

#define N_FILES 10
#define BLOCKSIZE 10000
#define FSPATHLEN 256
#define ITERS 100
#define FILEPERM 0666
#define DIRPERM 0755

char buf[BLOCKSIZE];

int main(int argc, char **argv) {
	int i, fd = 0, ret = 0;
	struct stat st;

	crfsinit(USE_DEFAULT_PARAM, USE_DEFAULT_PARAM, USE_DEFAULT_PARAM);

	/* TEST 1: file create test */
	if ((fd = open(TESTDIR "/dump.rdb", O_CREAT | O_RDWR, FILEPERM)) < 0) {
		perror("creat");
		printf("TEST 1: File create failure \n");
		exit(1);
	}
	printf("TEST 1: File create Success \n");


	/* TEST 2: file small write test */
	for (i = 0; i < ITERS; i++) {
		//memset with some random data
		memset(buf, 0x61 + i % 26, BLOCKSIZE);

		if (write(fd, buf, BLOCKSIZE) != BLOCKSIZE) {
			printf("TEST 2: File write failure \n");
			exit(1);
		}
	}
	
	/*fstat(fd, &st);
	if (st.st_size != ITERS*BLOCKSIZE) {
		printf("TEST 2: File write failure \n");
		exit(1);
	}*/
	printf("TEST 2: File write Success \n");


	/* TEST 3: file close */
	if (close(fd) < 0) {
		printf("TEST 3: File close failure \n");
	}
	printf("TEST 3: File close Success \n");


	/* Open for reading */
	if ((fd = open(TESTDIR "/dump.rdb", O_RDONLY)) < 0) {
		perror("open");
		exit(1);
	}

	/* TEST 4: file small read test */
	for (i = 0; i < ITERS; i++) {
		//clear buffer
		memset(buf, 0, BLOCKSIZE);

		if (read(fd, buf, BLOCKSIZE) != BLOCKSIZE) {
			printf("TEST 4: File read failure \n");
			exit(1);
		}
		
		if (buf[0] != 0x61 + i % 26) {
			printf("Mismatch, get %c, should be %c\n", buf[0], 0x61 + i % 26);
			printf("TEST 4: File read failure \n");
			exit(1);
		}
	}
        
	if (pread(fd, buf, BLOCKSIZE, 2*BLOCKSIZE) != BLOCKSIZE) {
		perror("pread");
		printf("TEST 4: File read failure \n");
		exit(1);
	} else if (buf[0] != 0x61 + 2 % 26) {
		printf("Mismatch, get %c, should be %c\n", buf[0], 0x61 + 2 % 26);
		printf("TEST 4: File read failure \n");
		exit(1);
	}
    
	printf("TEST 4: File read Success \n");
	close(fd);

	/* TEST 5: file remove test */
	if ((ret = unlink(TESTDIR "/dump.rdb")) < 0) {
		perror("unlink");
		printf("TEST 5: File unlink failure \n");
		exit(1);
	}
	printf("TEST 5: File unlink success \n");


	/* TEST 6: directory create test */
	if ((ret = mkdir(TESTDIR "/files", DIRPERM)) < 0) {
		perror("mkdir");
		printf("TEST 6: failure. Check if dir %s already exists, and "
			"if it exists, manually remove and re-run \n", TESTDIR "/files");
		exit(1);
	}
	printf("TEST 6: Directory create success \n");


	/* TEST 7: directory remove test */
	if ((ret = rmdir(TESTDIR "/files")) < 0) {
		perror("mkdir");
		printf("TEST 7: Directory remove failure \n");
		exit(1);
	}

	if (opendir(TESTDIR "/files") != NULL) {
		perror("mkdir");
		printf("TEST 7: Directory remove failure \n");
		exit(1);
	}

	printf("TEST 7: Directory remove success \n");


	mkdir(TESTDIR "/files", DIRPERM);

	/* TEST 8: sub-directory create test */
	for (i = 0; i < N_FILES; ++i) {
		char subdir_path[FSPATHLEN];
		memset(subdir_path, 0, FSPATHLEN);

		sprintf(subdir_path, "%s%d", TESTDIR "/files/dir", i);
		if ((ret = mkdir(subdir_path, DIRPERM)) < 0) {
			perror("mkdir");
			printf("TEST 8: Sub-directory create failure \n");
			exit(1);
		}
	}
	
	for (i = 0; i < N_FILES; ++i) {
		DIR *dir;
		char subdir_path[FSPATHLEN];
		memset(subdir_path, 0, FSPATHLEN);

		sprintf(subdir_path, "%s%d", TESTDIR "/files/dir", i);
		if ((dir = opendir(subdir_path)) == NULL) {
			perror("opendir");
			printf("TEST 8: Sub-directory create failure \n");
			exit(1);
		}
	}
	printf("TEST 8: Sub-directory create success \n");
#if 0
	/* TEST 9: glibc functions test */
	FILE *fp = fopen(TESTDIR"/testglibc", "w");

	strcpy(buf, "This is first Line\n");
	fwrite(buf, 1, strlen(buf), fp);

	strcpy(buf, "This is second Line\n");
	fwrite(buf, 1, strlen(buf), fp);

	strcpy(buf, "SELECT\n");
	fwrite(buf, 1, strlen(buf), fp);

	fclose(fp);

	fp = fopen(TESTDIR"/testglibc", "r");
	memset(buf, 0, 4096);

	fgets(buf, 4096, fp);
	printf("buf = %s\n", buf);
	
	long offset = ftell(fp);
	printf("after fgets, offset = %ld\n", offset);

	/*fgets(buf, 4096, fp);
	printf("buf = %s\n", buf);*/

	/*int cnt = 0;
	while (fgetc(fp) != EOF)
		++cnt;

	printf("cnt = %d\n", cnt);*/

	int re = fread(buf, 2, 1, fp);
	printf("buf = %s, re = %d\n", buf, re);

	offset = ftell(fp);
	printf("after fread, offset = %ld\n", offset);

	/*while (fgets(buf, 4096, fp) != NULL)
		printf("buf = %s\n", buf);*/

	if (feof(fp) == 0)
		printf("That's bad...\n");

	fclose(fp);
#endif
	crfsexit();

	printf("Benchmark completed \n");
	return 0;
}
