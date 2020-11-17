all: crfs_client crfs_client_stat crfs_client_fsync crfs_client_sharefd crfs_client_openclose crfs_client_openclose_posix crfs_client_posixio crfs_client_stat_posixio crfs_client_fsync_posixio test_shim crfs_exit
SYSCALL_INTERCEPT_DIR := $(abspath $(CURDIR)/libshim/syscall_intercept/install/)

CODE=.
CXX=g++
CC=gcc
LIB_S=-lpthread -lrt -L./libfs/ -lcrfslib -L./libfs/interval_tree -lintervaltreelib -lnvmchkpt
FLAGS+=-D_USE_DEVFS_LIB
FLAGS+=-D_USE_DEVFS_CALLS
INCLUDE=-I./libfs/

crfs_client: crfs_client.c
	$(CC) $(INCLUDE) $(FLAGS) crfs_client.c -o crfs_client $(LIB_S)

crfs_client_stat: crfs_client_stat.c
	$(CC) $(INCLUDE) $(FLAGS) crfs_client_stat.c -o crfs_client_stat $(LIB_S)

crfs_client_fsync: crfs_client_fsync.c
	$(CC) $(INCLUDE) $(FLAGS) crfs_client_fsync.c -o crfs_client_fsync $(LIB_S)

crfs_client_sharefd: crfs_client_sharefd.c
	$(CC) $(INCLUDE) $(FLAGS) crfs_client_sharefd.c -o crfs_client_sharefd $(LIB_S)

crfs_client_openclose: crfs_client_openclose.c
	$(CC) $(INCLUDE) $(FLAGS) crfs_client_openclose.c -o crfs_client_openclose $(LIB_S)

crfs_client_openclose_posix: crfs_client_openclose_posix.c
	$(CC) crfs_client_openclose_posix.c -o crfs_client_openclose_posix $(LIB_S)

crfs_client_posixio: crfs_client_posixio.c
	$(CC) crfs_client_posixio.c -o crfs_client_posixio -lpthread

crfs_client_stat_posixio: crfs_client_stat_posixio.c
	$(CC) crfs_client_stat_posixio.c -o crfs_client_stat_posixio -lpthread

crfs_client_fsync_posixio: crfs_client_fsync_posixio.c
	$(CC) $(FLAGS) crfs_client_fsync_posixio.c -o crfs_client_fsync_posixio $(LIB_S)

crfs_exit: crfs_exit.c
	$(CC) $(FLAGS) crfs_exit.c -o crfs_exit $(LIB_S)

test_shim: test_shim.c
	$(CC) $(INCLUDE) test_shim.c -o test_shim $(LIB_S)

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf crfs_client crfs_client_stat crfs_client_fsync crfs_client_sharefd crfs_client_openclose crfs_client_openclose_posix crfs_client_posixio crfs_client_stat_posixio crfs_client_fsync_posixio test_shim crfs_exit
	

