#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <pthread.h>

#include "unvme_nvme.h"
#include "crfslib.h"

#include <libsyscall_intercept_hook_point.h>

#define RECORD_PATH "io_syscall_record.txt"
#define RECORD_LENGTH 4096

#define OPEN_FILE_MAX 1048576
#define PATH_BUF_SIZE 4095
#define DEVFS_PREFIX (char *)"/mnt/ram"
#define syscall_trace(...)

#ifdef __cplusplus
// extern "C" {
#endif

static int crfs_fd_table[OPEN_FILE_MAX] = {0};
static int initialized = 0;
static FILE *rec_file = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

const char* shell_env = NULL;

// Only works on Linux
void record_syscall(int fd, char *ope, size_t count) {
	char record[RECORD_LENGTH];
	pid_t pid, tid;

	if (initialized == 0) {
		if ((rec_file = fopen(RECORD_PATH, "a")) == NULL) {
			exit(0);
		}

		sprintf(record, "Fd | PID | TID | Operation | IO size");
		fprintf(rec_file,"%s\n", record); 
	
		initialized = 1;
	}

	/* Get I/O TID PID */
	pid = getpid();
	tid = syscall(SYS_gettid);

	/* Insert this system call record to file */
	sprintf(record, "%d | %d | %d | %s | %lu", fd, pid, tid, ope, count);

	pthread_mutex_lock(&mutex);
	fprintf(rec_file,"%s\n", record);
	pthread_mutex_unlock(&mutex);
}


int shim_do_open(char *filename, int flags, mode_t mode, int* result)
{
  int ret;
  char fullpath[PATH_BUF_SIZE];

  if (flags > 0x4000)
    return 1;

  memset(fullpath, 0, PATH_BUF_SIZE);

  if (filename[0] == '/') {                                                                                                                                                
    strcpy(fullpath, filename);
  } else {
    getcwd(fullpath, sizeof(fullpath));
    strcat(fullpath, "/");
    strcat(fullpath, filename);
  }   

  if (strncmp(fullpath, "/mnt/ram", 8) ||
      !strcmp(fullpath, "/mnt/ram/test")){
    // /mn/ram/test is the device file for ioctl, need to be excluded from shim path
    return 1;
  } else {
    ret = crfsopen(fullpath, flags, mode);
    if (ret > 0) 
       crfs_fd_table[ret] = 1;

    shell_env = getenv("MULPROC");
    if (shell_env)
       crfs_fd_table[ret]++;

    syscall_trace(__func__, ret, 3, filename, flags, mode);

    *result = ret;
    return 0;
  }
}

int shim_do_openat(int dfd, const char *filename, int flags, mode_t mode, int* result)
{
  // not used

  return 1;
}

int shim_do_creat(char *filename, mode_t mode, int* result)
{
  int ret;
  char fullpath[PATH_BUF_SIZE];

  memset(fullpath, 0, PATH_BUF_SIZE);

  if (filename[0] == '/') {                                                                                                                                                
    strcpy(fullpath, filename);
  } else {
    getcwd(fullpath, sizeof(fullpath));
    strcat(fullpath, "/");
    strcat(fullpath, filename);
  }   

  if (strncmp(fullpath, "/mnt/ram", 8) ||
      !strcmp(fullpath, "/mnt/ram/test")){

    return 1;
  } else {
    ret = crfsopen(fullpath, O_CREAT, mode);
    if (ret > 0) 
       crfs_fd_table[ret] = 1;

    syscall_trace(__func__, ret, 2, filename, mode);

    *result = ret;
    return 0;
  }
}

int shim_do_read(int fd, void *buf, size_t count, size_t* result)
{
  size_t ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfsread(fd, buf, count);

    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_pread64(int fd, void *buf, size_t count, loff_t off, size_t* result)
{
  size_t ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfspread(fd, buf, count, off);

    syscall_trace(__func__, ret, 4, fd, buf, count, off);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_write(int fd, void *buf, size_t count, size_t* result)
{
  size_t ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfswrite(fd, buf, count);

    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_pwrite64(int fd, void *buf, size_t count, loff_t off, size_t* result)
{
  size_t ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfspwrite(fd, buf, count, off);

    syscall_trace(__func__, ret, 4, fd, buf, count, off);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_close(int fd, int* result)
{
  int ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfsclose(fd);
    crfs_fd_table[fd] = 0; 

    syscall_trace(__func__, ret, 1, fd);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_lseek(int fd, off_t offset, int origin, int* result)
{
  int ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfslseek64(fd, offset, origin);
    syscall_trace(__func__, ret, 3, fd, offset, origin);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_mkdir(void *path, mode_t mode, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name((char *)path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    //printf("%s: go to mlfs\n", path_buf);
    ret = mlfs_posix_mkdir(path_buf, mode);
    syscall_trace(__func__, ret, 2, path, mode);

    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_rmdir(const char *path, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_rmdir((char *)path);
    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_rename(char *oldname, char *newname, int* result)
{
  return 1;

  int ret;
  char fullpathold[PATH_BUF_SIZE];
  char fullpathnew[PATH_BUF_SIZE];

  memset(fullpathold, 0, PATH_BUF_SIZE);
  memset(fullpathnew, 0, PATH_BUF_SIZE);

  if (oldname[0] == '/') {
    strcpy(fullpathold, oldname);
  } else {
    getcwd(fullpathold, sizeof(fullpathold));
    strcat(fullpathold, "/");
    strcat(fullpathold, oldname);
  }

  if (newname[0] == '/') {
    strcpy(fullpathnew, newname);
  } else {
    getcwd(fullpathnew, sizeof(fullpathnew));
    strcat(fullpathnew, "/");
    strcat(fullpathnew, newname);
  }

  if (strncmp(fullpathold, "/mnt/ram", 8) ||
      !strcmp(fullpathold, "/mnt/ram/test")){
    return 1;
  } else {
    ret = crfsrename(fullpathold, fullpathnew);
    syscall_trace(__func__, ret, 2, oldname, newname);
    *result = ret;
    return 0;
  }


  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(oldname, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_rename(oldname, newname);
    syscall_trace(__func__, ret, 2, oldname, newname);

    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_fallocate(int fd, int mode, off_t offset, off_t len, int* result)
{
  int ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfsfallocate(fd, mode, offset, len);
    syscall_trace(__func__, ret, 4, fd, mode, offset, len);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_stat(const char *filename, struct stat *statbuf, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_stat(filename, statbuf);
    syscall_trace(__func__, ret, 2, filename, statbuf);

    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_lstat(const char *filename, struct stat *statbuf, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    // Symlink does not implemented yet
    // so stat and lstat is identical now.
    ret = mlfs_posix_stat(filename, statbuf);
    syscall_trace(__func__, ret, 2, filename, statbuf);

    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_fstat(int fd, struct stat *statbuf, int* result)
{
  /*int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_fstat(get_mlfs_fd(fd), statbuf);
    syscall_trace(__func__, ret, 2, fd, statbuf);

    *result = ret;
    return 0;
  } else {
    return 1;
  }*/
  return 1;
}

int shim_do_truncate(const char *filename, off_t length, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_truncate(filename, length);
    syscall_trace(__func__, ret, 2, filename, length);

    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_ftruncate(int fd, off_t length, int* result)
{
  int ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfsftruncate(fd, length);
    syscall_trace(__func__, ret, 2, fd, length);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_unlink(const char *path, int* result)
{
  return 1;

  int ret;
  char fullpath[PATH_BUF_SIZE];

  memset(fullpath, 0, PATH_BUF_SIZE);

  if (path[0] == '/') {
    strcpy(fullpath, path);
  } else {
    getcwd(fullpath, sizeof(fullpath));
    strcat(fullpath, "/");
    strcat(fullpath, path);
  }

  if (strncmp(fullpath, "/mnt/ram", 8) ||
      !strcmp(fullpath, "/mnt/ram/test")){
    return 1;
  } else {
    ret = crfsunlink(fullpath);
    syscall_trace(__func__, ret, 1, path);
    *result = ret;
    return 0;
  }
}

int shim_do_symlink(const char *target, const char *linkpath, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(target, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    printf("%s\n", target);
    printf("symlink: do not support yet\n");
    exit(-1);
  }*/
  return 1;
}

int shim_do_access(const char *pathname, int mode, int* result)
{
  /*int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(pathname, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, 5) != 0){
    return 1;
  } else {
    ret = mlfs_posix_access((char *)pathname, mode);
    syscall_trace(__func__, ret, 2, pathname, mode);

    *result = ret;
    return 0;
  }*/
  return 1;
}

int shim_do_fsync(int fd, int* result)
{
  int ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfsfsync(fd);    

    syscall_trace(__func__, 0, 1, fd);

    *result = ret;
    return 0;
  } else {
    return 1;
  }
}

int shim_do_fdatasync(int fd, int* result)
{
  int ret;

  if (crfs_fd_table[fd] == 1) {
    ret = crfsfsync(fd);    

    syscall_trace(__func__, 0, 1, fd);

    *result = ret;
    return 0;
  } else {
    return 1;
  }

}

int shim_do_sync(int* result)
{
  int ret;

  printf("sync: do not support yet\n");
  exit(-1);
}

int shim_do_fcntl(int fd, int cmd, void *arg, int* result)
{
  /*int ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_fcntl(get_mlfs_fd(fd), cmd, arg);
    syscall_trace(__func__, ret, 3, fd, cmd, arg);

    *result = ret;
    return 0;
  } else {
    return 1;
  }*/
  return 1;
}

int shim_do_mmap(void *addr, size_t length, int prot,
                   int flags, int fd, off_t offset, void** result)
{
  /*void* ret;

  if (check_mlfs_fd(fd)) {
    printf("mmap: not implemented\n");
    exit(-1);
  } else {
    return 1;
  }*/
  return 1;
}

int shim_do_munmap(void *addr, size_t length, int* result)
{
  return 1;

}

#if 0
int shim_do_getdents(int fd, struct linux_dirent *buf, size_t count, size_t* result)
{
  /*size_t ret;

  if (check_mlfs_fd(fd)) {
    ret = mlfs_posix_getdents(get_mlfs_fd(fd), buf, count);

    syscall_trace(__func__, ret, 3, fd, buf, count);

    *result = ret;
    return 0;
  } else {
    return 1;
  }*/
  return 1;
}

int shim_do_getdents64(int fd, struct linux_dirent64 *buf, size_t count, size_t* result)
{
  /*size_t ret;

  if (check_mlfs_fd(fd)) {
    printf("getdent64 is not supported\n");
    exit(-1);
  } else {
    return 1;
  }*/
  return 1;
}
#endif

static int
hook(long syscall_number,
     long arg0, long arg1,
     long arg2, long arg3,
     long arg4, long arg5,
     long *result) {
  switch (syscall_number) {
    case SYS_open: return shim_do_open((char*)arg0, (int)arg1, (mode_t)arg2, (int*)result);
    case SYS_openat: return shim_do_openat((int)arg0, (const char*)arg1, (int)arg2, (mode_t)arg3, (int*)result);
    case SYS_creat: return shim_do_creat((char*)arg0, (mode_t)arg1, (int*)result);
    case SYS_read: return shim_do_read((int)arg0, (void*)arg1, (size_t)arg2, (size_t*)result);
    case SYS_pread64: return shim_do_pread64((int)arg0, (void*)arg1, (size_t)arg2, (loff_t)arg3, (size_t*)result);
    case SYS_write: return shim_do_write((int)arg0, (void*)arg1, (size_t)arg2, (size_t*)result);
    case SYS_pwrite64: return shim_do_pwrite64((int)arg0, (void*)arg1, (size_t)arg2, (loff_t)arg3, (size_t*)result);
    case SYS_close: return shim_do_close((int)arg0, (int*)result);
    case SYS_lseek: return shim_do_lseek((int)arg0, (off_t)arg1, (int)arg2, (int*)result);
    case SYS_mkdir: return shim_do_mkdir((void*)arg0, (mode_t)arg1, (int*)result);
    case SYS_rmdir: return shim_do_rmdir((const char*)arg0, (int*)result);
    case SYS_rename: return shim_do_rename((char*)arg0, (char*)arg1, (int*)result);
    case SYS_fallocate: return shim_do_fallocate((int)arg0, (int)arg1, (off_t)arg2, (off_t)arg3, (int*)result);
    case SYS_stat: return shim_do_stat((const char*)arg0, (struct stat*)arg1, (int*)result);
    case SYS_lstat: return shim_do_lstat((const char*)arg0, (struct stat*)arg1, (int*)result);
    case SYS_fstat: return shim_do_fstat((int)arg0, (struct stat*)arg1, (int*)result);
    case SYS_truncate: return shim_do_truncate((const char*)arg0, (off_t)arg1, (int*)result);
    case SYS_ftruncate: return shim_do_ftruncate((int)arg0, (off_t)arg1, (int*)result);
    case SYS_unlink: return shim_do_unlink((const char*)arg0, (int*)result);
    case SYS_symlink: return shim_do_symlink((const char*)arg0, (const char*)arg1, (int*)result);
    case SYS_access: return shim_do_access((const char*)arg0, (int)arg1, (int*)result);
    case SYS_fsync: return shim_do_fsync((int)arg0, (int*)result);
    case SYS_fdatasync: return shim_do_fdatasync((int)arg0, (int*)result);
    case SYS_sync: return shim_do_sync((int*)result);
    case SYS_fcntl: return shim_do_fcntl((int)arg0, (int)arg1, (void*)arg2, (int*)result);
    case SYS_mmap: return shim_do_mmap((void*)arg0, (size_t)arg1, (int)arg2, (int)arg3, (int)arg4, (off_t)arg5, (void**)result);
    case SYS_munmap: return shim_do_munmap((void*)arg0, (size_t)arg1, (int*)result);
    //case SYS_getdents: return shim_do_getdents((int)arg0, (struct linux_dirent*)arg1, (size_t)arg2, (size_t*)result);
    //case SYS_getdents64: return shim_do_getdents64((int)arg0, (struct linux_dirent64*)arg1, (size_t)arg2, (size_t*)result);
  }
  return 1;
}

static __attribute__((constructor)) void init(void)
{
  // Set up the callback function
  intercept_hook_point = hook;
}


#ifdef __cplusplus
// }
#endif
