#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

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
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "crfslibio.h"

static int delcounter;

/* Function pointers to hold the value of the glibc functions */
static ssize_t (*real_write)(int fd, const void *buf, size_t count) = NULL;
static ssize_t (*real_read)(int fd, void *buf, size_t count) = NULL;
static ssize_t (*real_pwrite)(int fd, const void *buf, size_t count, off_t offset) = NULL;
static ssize_t (*real_pread)(int fd, void *buf, size_t count, off_t offset) = NULL;

static int (*real_open)(const char *pathname, int flags, mode_t mode) = NULL;
static int (*real_close)(int fd) = NULL;
static int (*real_lseek64)(int fd, off_t offset, int whence) = NULL;

static int (*real_unlink)(const char *pathname) = NULL;
static int (*real_rename)(const char *oldpath, const char *newpath) = NULL;
static int (*real_fsync)(int fd) = NULL;

/* Initialize DevFS */
int crfsinit(unsigned int qentry_count,
		unsigned int dev_core_cnt, unsigned int sched_policy) {
#if !defined(_POSIXIO)
	initialize_crfs(qentry_count, dev_core_cnt, sched_policy);
#endif
	return 0;
}

/* Shutdown DevFS */
int crfsexit(void) {
#if !defined(_POSIXIO)
	shutdown_crfs();
#endif
	return 0;
}

/* wrapping write function call */
int crfslseek64(int fd, off_t offset, int whence) {
	int ret = 0;

#if !defined(_POSIXIO)
	ret = crfs_lseek64(fd, offset, whence);
	return ret;
#else
	real_lseek64 = dlsym(RTLD_NEXT, "lseek64");
	ret = real_lseek64(fd, offset, whence);
	return ret;
#endif
}


/* wrapping write function call */
ssize_t crfswrite(int fd, const void *buf, size_t count) {
	size_t sz;

#if !defined(_POSIXIO)
	sz = crfs_write(fd, buf, count);
	return sz;
#else
	real_write = dlsym(RTLD_NEXT, "write");
	sz = real_write(fd, buf, count);
	return sz;	
#endif
}

ssize_t crfspwrite(int fd, const void *buf, size_t count, off_t offset) {
	size_t sz;

#if !defined(_POSIXIO)
	sz = crfs_pwrite(fd, buf, count, offset);
	return sz;
#else
	real_pwrite = dlsym(RTLD_NEXT, "pwrite");
	sz = real_pwrite(fd, buf, count, offset);
	return sz;	
#endif
}


/* wrapping read function call */
ssize_t crfsread(int fd, void *buf, size_t count) {
	size_t sz;
	
#if !defined(_POSIXIO)
	sz = crfs_read(fd, buf, count);
	return sz;
#else
	real_read = dlsym(RTLD_NEXT, "read");
	sz = real_read(fd, buf, count);
	return sz;
#endif
}


ssize_t crfspread(int fd, void *buf, size_t count, off_t offset) {
	size_t sz;
#if !defined(_POSIXIO)
	sz = crfs_pread(fd, buf, count, offset);
	return sz;
#else
	real_pread = dlsym(RTLD_NEXT, "pread");
	sz = real_pread(fd, buf, count, offset);
	return sz;
#endif
}


/* wrapping open function call */
int crfsopen(const char *pathname, int flags, mode_t mode)
{
	int fd = -1;

#if !defined(_POSIXIO)
	fd = crfs_open_file(pathname, flags, mode);
#else
	real_open = dlsym(RTLD_NEXT, "open");
	real_open(pathname, flags, mode);
#endif
	return fd;
}


/* wrapping unlink function call */
int crfsunlink(const char *pathname)
{
	int ret = 0;

#if !defined(_POSIXIO)
	ret = crfs_unlink(pathname);
#else
	real_unlink = dlsym(RTLD_NEXT, "unlink");
	real_unlink(pathname);
#endif
	return ret;
}

/* wrapping fsync function call */
int crfsfsync(int fd)
{
	int ret = 0;
#if !defined(_POSIXIO)
	ret = crfs_fsync(fd);
#else
	real_fsync = dlsym(RTLD_NEXT, "fsync");
	ret = real_fsync(fd);
#endif
	return ret;
}


/* wrapping open function call */
int crfsclose(int fd)
{

#if defined(_DEBUG)
	printf("Close#:%d\n", fd);
#endif

#if !defined(_POSIXIO)
	return crfs_close_file(fd);
#else
	real_close = dlsym(RTLD_NEXT, "close");
	real_close(fd);
#endif
}


int crfsfallocate(int fd, int mode, off_t offset, off_t len)
{
        int ret = 0;
#if !defined(_POSIXIO)
        ret = crfs_fallocate(fd, offset, len);
#else
        real_fallocate = dlsym(RTLD_NEXT, "fallocate");
        ret = real_fallocate(fd, mode, offset, len);
#endif
        return ret;

}


int crfsftruncate(int fd, off_t length)
{
        int ret = 0;
#if !defined(_POSIXIO)
        ret = crfs_ftruncate(fd, length);
#else
        real_ftruncate = dlsym(RTLD_NEXT, "ftruncate");
        ret = real_ftruncate(fd, length);
#endif
        return ret;
}


int crfsrename(const char *oldpath, const char *newpath)
{
        int ret = 0;
#if !defined(_POSIXIO)
        ret = crfs_rename(oldpath, newpath);
#else
        real_rename = dlsym(RTLD_NEXT, "rename");
        ret = real_rename(oldpath, newpath);
#endif
        return ret;

}

