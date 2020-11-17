#ifndef DEVFSLIB_H
#define DEVFSLIB_H

#define USE_DEFAULT_PARAM 0

#ifdef __cplusplus
extern "C" {
	int crfsinit(unsigned int qentry_count,
		unsigned int dev_core_cnt, unsigned int sched_policy);
	int crfsexit(void);
}
#else
int crfsinit(unsigned int qentry_count,
		unsigned int dev_core_cnt, unsigned int sched_policy);
int crfsexit(void);
#endif

ssize_t crfswrite(int fd, const void *buf, size_t count);
ssize_t crfsread(int fd, void *buf, size_t count);
ssize_t crfspwrite(int fd, const void *buf, size_t count, off_t offset);
ssize_t crfspread(int fd, void *buf, size_t count, off_t offset);
int crfsclose(int fd);
int crfslseek64(int fd, off_t offset, int whence);
int crfsopen(const char *pathname, int flags, mode_t mode);
int crfsunlink(const char *pathname);
int crfsfsync(int fd);
int crfsfallocate(int fd, int mode, off_t offset, off_t len);
int crfsftruncate(int fd, off_t length);
int crfsrename(const char *oldpath, const char *newpath);

#endif
