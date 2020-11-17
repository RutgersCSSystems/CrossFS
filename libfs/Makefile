all: crfslib

CODE=.
CXX=g++
CC=gcc
LIB_S=-lpthread -lrt -L./ -lcrfslib -L./interval_tree -lintervaltreelib
#FLAGS+=-D_KERNEL_TESTING
#FLAGS+=-D_POSIXIO
#FLAGS+=-g
#FLAGS+=-D_DEBUG
FLAGS+=-D_USE_DEVFS_LIB -D_USE_DEVFS_CALLS #-D_USE_THREADING
#FLAGS+=-D_VERIFY_DATA
#FLAGS+=-DTEMP_SCALE
#FLAGS+=-DSHADOW_FD
FLAGS+=-DPARAFS_SCHED_THREAD
FLAGS+=-DPARAFS_BYPASS_KERNEL
#FLAGS+=-D_USE_OPT
#FLAGS+=-DPCIE_DELAY
#FLAGS+=-DFLUSH_CACHE
FLAGS+=-DPARAFS_FSYNC_ENABLE
FLAGS+=-DPARAFS_INTERVAL_TREE
#FLAGS+=-DPARAFS_STAT
#FLAGS+=-DREDUCE_CPU_FREQ
#FLAGS+=-D_NVMFDQ
#FLAGS+=-DPARAFS_SHM
FLAGS+=-DCRFS_OPENCLOSE_OPT

obj-m := libcrfslib.o
KDIR := /lib/modules/`uname -r`/build
PWD := `pwd`

crfslib:
	sudo cp uthash/*.h /usr/local/include
	sudo cp uthash/*.h /usr/local/include
	sudo cp uthash/*.h /usr/lib/
	make -C interval-tree
	make install -C interval-tree
	gcc -c -fPIC $(FLAGS) crfslibio.c -lpthread -lnvmchkpt
	gcc -fPIC -shared $(FLAGS) -o libcrfslib.so crfslib.c crfslibio.o -ldl -lpthread -lrt -lmm -lnvmchkpt

install:
	sudo cp *.so /usr/local/lib
	sudo cp *.so /usr/lib
	sudo cp *.h /usr/local/include
	sudo cp *.h /usr/include
	sudo cp *.h /usr/lib/

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf *.so
	rm -rf interval-tree/*.o
	rm -rf interval-tree/*.so
	rm -rf interval-tree/interval_csv interval-tree/interval-tree-test
