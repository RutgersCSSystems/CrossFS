#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "interfaces.h"
#include "shim_types.h"
#include "shim_syscall_macro.h"
#include "shim_sys_fs.h"

#include "unvme_nvme.h"
#include "crfslib.h"

#define DEVFS_PREFIX "/mnt/ram"

/* System call ABIs
 * syscall number : %eax
 * parameter sequence: %rdi, %rsi, %rdx, %rcx, %r8, %r9
 * Kernel destroys %rcx, %r11
 * return : %rax
 */

//#define PATH_BUF_SIZE 4095
//#define MLFS_PREFIX (char *)"/mlfs"

#ifdef __cplusplus
extern "C" {
#endif

static int crfs_fd_table[FOPEN_MAX] = {0};

#if 0
static int collapse_name(const char *input, char *_output)
{
	char *output = _output;

	while(1) {
		/* Detect a . or .. component */
		if (input[0] == '.') {
			if (input[1] == '.' && input[2] == '/') {
				/* A .. component */
				if (output == _output)
					return -1;
				input += 2;
				while (*(++input) == '/');
				while(--output != _output && *(output - 1) != '/');
				continue;
			} else if (input[1] == '/') {
				/* A . component */
				input += 1;
				while (*(++input) == '/');
				continue;
			}
		}

		/* Copy from here up until the first char of the next component */
		while(1) {
			*output++ = *input++;
			if (*input == '/') {
				*output++ = '/';
				/* Consume any extraneous separators */
				while (*(++input) == '/');
				break;
			} else if (*input == 0) {
				*output = 0;
				return output - _output;
			}
		}
	}
}
#endif

int shim_do_open(char *filename, int flags, mode_t mode)
{
	int ret;

	int fd = 0;
	char fullpath[PATH_MAX];

	if (filename[0] == '/') {
		strcpy(fullpath, filename);
	} else {
		getcwd(fullpath, sizeof(fullpath));
		strcat(fullpath, "/");
		strcat(fullpath, filename);
	}

	if (!strcmp(fullpath, "/mnt/ram/test"))
        	goto default_open;

	if (strncmp(fullpath, "/mnt/ram", 8)) {
        	goto default_open;
	} else {
		fd = crfsopen(fullpath, flags, mode);
		if (fd > 0)
            		crfs_fd_table[fd] = 1;
		syscall_trace(__func__, ret, 3, filename, flags, mode);
		return fd;
	}
	
default_open:
	asm("mov %1, %%rdi;"
		"mov %2, %%esi;"
		"mov %3, %%edx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fullpath), "r"(flags), "r"(mode), "r"(__NR_open)
		:"rax", "rdi", "rsi", "rdx"
		);

	return ret;
}

int shim_do_openat(int dfd, const char *filename, int flags, mode_t mode)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	//printf("...openat\n");	

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%edx;"
		"mov %4, %%r10d;"
		"mov %5, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(dfd),"r"(filename), "r"(flags), "r"(mode), "r"(__NR_openat)
		:"rax", "rdi", "rsi", "rdx", "r10"
		);

	return ret;
}

int shim_do_creat(char *filename, mode_t mode)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	//printf("...creat\n");	

	asm("mov %1, %%rdi;"
		"mov %2, %%esi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(filename), "r"(mode), "r"(__NR_creat)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

size_t shim_do_read(int fd, void *buf, size_t count)
{
	int ret;

	if (crfs_fd_table[fd] == 1) {
		ret = crfsread(fd, buf, count);
		syscall_trace(__func__, ret, 3, fd, buf, count);
		return ret;
	}

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%rdx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(buf), "r"(count), "r"(__NR_read)
		:"rax", "rdi", "rsi", "rdx"
		);

	return ret;
}

size_t shim_do_pread64(int fd, void *buf, size_t count, loff_t off)
{
	int ret;

	if (crfs_fd_table[fd] == 1) {
        ret = crfspread(fd, buf, count, off);
		syscall_trace(__func__, ret, 4, fd, buf, count, off);
		return ret;
	}

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%rdx;"
		"mov %4, %%r10;"
		"mov %5, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(buf), "r"(count), "r"(off), "r"(__NR_pread64)
		:"rax", "rdi", "rsi", "rdx", "r10"
		);

	return ret;
}

size_t shim_do_write(int fd, void *buf, size_t count)
{
	int ret;

    if (crfs_fd_table[fd] == 1) {
        ret = crfswrite(fd, buf, count);
		syscall_trace(__func__, ret, 3, fd, buf, count);
		return ret;
	}

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%rdx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(buf), "r"(count), "r"(__NR_write)
		:"rax", "rdi", "rsi", "rdx"
		);

	return ret;
}

size_t shim_do_pwrite64(int fd, void *buf, size_t count, loff_t off)
{
	int ret;

	//printf("...pwrite64\n");	

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%rdx;"
		"mov %4, %%r10;"
		"mov %5, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(buf), "r"(count),"r"(off), "r"(__NR_pwrite64)
		:"rax", "rdi", "rsi", "rdx", "r10"
		);

	return ret;
}

int shim_do_close(int fd)
{
	int ret;

	if (crfs_fd_table[fd] == 1) {
		ret = crfsclose(fd);
		crfs_fd_table[fd] = 0;
		syscall_trace(__func__, ret, 1, fd);
		return ret;
	} 

	asm("mov %1, %%edi;"
		"mov %2, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(__NR_close)
		:"rax", "rdi"
		);

	return ret;
}

int shim_do_lseek(int fd, off_t offset, int origin)
{
	int ret;

	//printf("...lseek\n");

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%edx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(offset), "r"(origin), "r"(__NR_lseek)
		:"rax", "rdi", "rsi", "rdx"
		);

	return ret;
}

int shim_do_mkdir(void *path, mode_t mode)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	//printf("...mkdir\n");

	asm("mov %1, %%rdi;"
		"mov %2, %%esi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(path), "r"(mode), "r"(__NR_mkdir)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_rmdir(const char *path)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	//printf("...rmdir\n");

	asm("mov %1, %%rdi;"
		"mov %2, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(path), "r"(__NR_rmdir)
		:"rax", "rdi"
		);

	return ret;
}

int shim_do_rename(char *oldname, char *newname)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	//printf("...rename\n");

	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(oldname), "m"(newname), "r"(__NR_rename)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_fallocate(int fd, int mode, off_t offset, off_t len)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%esi;"
		"mov %3, %%rdx;"
		"mov %4, %%r10;"
		"mov %5, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(mode), "r"(offset), "r"(len), "r"(__NR_fallocate)
		:"rax", "rdi", "rsi", "rdx", "r10"
		);

	return ret;
}

int shim_do_stat(const char *filename, struct stat *statbuf)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
		
	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(filename), "m"(statbuf), "r"(__NR_stat)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_lstat(const char *filename, struct stat *statbuf)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
			
	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(filename), "m"(statbuf), "r"(__NR_lstat)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_fstat(int fd, struct stat *statbuf)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(statbuf), "r"(__NR_fstat)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_truncate(const char *filename, off_t length)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
			
	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(filename), "r"(length), "r"(__NR_truncate)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_ftruncate(int fd, off_t length)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(length), "r"(__NR_ftruncate)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_unlink(const char *path)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	//printf("...unlink\n");

	asm("mov %1, %%rdi;"
		"mov %2, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(path), "r"(__NR_unlink)
		:"rax", "rdi"
		);

	return ret;
}

int shim_do_symlink(const char *target, const char *linkpath)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
	
	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(target), "m"(linkpath), "r"(__NR_symlink)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_access(const char *pathname, int mode)
{
	int ret;
	//char path_buf[PATH_BUF_SIZE];
			
	asm("mov %1, %%rdi;"
		"mov %2, %%esi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(pathname), "r"(mode), "r"(__NR_access)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

int shim_do_fsync(int fd)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(__NR_fsync)
		:"rax", "rdi"
		);

	return ret;
}

int shim_do_fdatasync(int fd)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(__NR_fdatasync)
		:"rax", "rdi"
		);

	return ret;
}

int shim_do_sync(void)
{
	int ret;

	//printf("sync: do not support yet\n");
	exit(-1);

	asm("mov %1, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(__NR_sync)
		:"rax" 
		);

	return ret;
}

int shim_do_fcntl(int fd, int cmd, void *arg)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%esi;"
		"mov %3, %%rdx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "r"(cmd), "m"(arg), "r"(__NR_fcntl)
		:"rax", "rdi", "rsi", "rdx"
		);

	return ret;
}

void* shim_do_mmap(void *addr, size_t length, int prot, 
		int flags, int fd, off_t offset)
{
	void* ret;

	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%edx;"
		"mov %4, %%r10d;"
		"mov %5, %%r8d;"
		"mov %6, %%r9;"
		"mov %7, %%eax;"
		"syscall;\n\t"
		"mov %%rax, %0;\n\t"
		:"=r"(ret)
		:"m"(addr), "r"(length), "r"(prot), "r"(flags), "r"(fd), "r"(offset), "r"(__NR_mmap)
		:"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"
		);

	return ret;
}

int shim_do_munmap(void *addr, size_t length)
{
	int ret;

	asm("mov %1, %%rdi;"
		"mov %2, %%rsi;"
		"mov %3, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"m"(addr), "r"(length), "r"(__NR_munmap)
		:"rax", "rdi", "rsi"
		);

	return ret;
}

#if 0
size_t shim_do_getdents(int fd, struct linux_dirent *buf, size_t count)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%rdx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(buf), "r"(count), "r"(__NR_getdents)
		:"rax", "rdi", "rsi", "rdx", "r10"
		);

	return ret;
}

size_t shim_do_getdents64(int fd, struct linux_dirent64 *buf, size_t count)
{
	int ret;

	asm("mov %1, %%edi;"
		"mov %2, %%rsi;"
		"mov %3, %%rdx;"
		"mov %4, %%eax;"
		"syscall;\n\t"
		"mov %%eax, %0;\n\t"
		:"=r"(ret)
		:"r"(fd), "m"(buf), "r"(count), "r"(__NR_getdents)
		:"rax", "rdi", "rsi", "rdx", "r10"
		);

	return ret;
}
#endif

#ifdef __cplusplus
}
#endif
