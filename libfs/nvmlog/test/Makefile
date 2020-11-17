CXX=g++ 
CC=gcc
GNUFLAG :=  -std=gnu99 

INCLUDE=$(NVMALLOC_HOME) 


DEFINES := -DMALLOC_PRODUCTION -lstdc++


#CFLAGS := -g -I$(INCLUDE) -Wall -pipe -fvisibility=hidden \
      -funroll-loops  -Wno-implicit -Wno-uninitialized \
      -Wno-unused-function -Wno-unused-variable -lm -lssl  -fPIC

CFLAGS := -g -I$(INCLUDE) -Wall -pipe -fvisibility=hidden \
     -funroll-loops -lm -lssl  -fPIC -I$(NVMALLOC_HOME)/pvmobj -I$(NVMALLOC_HOME)/jemalloc

#LIBS= -ljemalloc -lpthread
LIBS= -lpthread -L$(NVMALLOC_HOME) -lnvmchkpt

all: nvm_rw_test snappy_test nonpersist_alloc

nonpersist_alloc: nonpersist_alloc.cc
	$(CXX) nonpersist_alloc.cc -o nonpersist_alloc $(CFLAGS) $(CFLAGS_STRICT) $(LIBS)  $(DEFINES)
nvm_rw_test: nvm_rw_test.cc
	$(CXX) nvm_rw_test.cc -o nvm_rw_test  $(CFLAGS) $(LIBS) -I$(INCLUDE) $(DEFINES)
snappy_test:  snappy_test.cc
	$(CXX) snappy_test.cc -o snappy_test  $(CFLAGS) $(LIBS) $(DEFINES) -lsnappy
nv_read_test:  nv_read_test.cc
	$(CXX) nv_read_test.cc -o nv_read_test  $(CFLAGS) $(LIBS) $(DEFINES)
varname_alloc_test: varname_alloc_test.cc
	$(CXX)	varname_alloc_test.cc -o varname_alloc_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
varname_read_test: varname_read_test.cc
	$(CXX)	varname_read_test.cc -o varname_read_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
varname_commit_test: varname_commit_test.cc
	$(CXX)	varname_commit_test.cc -o varname_commit_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
nv_chckpt_test: nv_chckpt_test.cc
	$(CXX) nv_chckpt_test.cc -o nv_chckpt_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
nv_restart_test:  nv_restart_test.cc
	$(CXX) nv_restart_test.cc -o nv_restart_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
nvmap_bench: nvmap_bench.cc
	$(CXX) nvmap_bench.cc -o nvmap_bench  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
nvmalloc_c_test: nvmalloc_c_test.c
	$(CC) nvmalloc_c_test.c -o nvmalloc_c_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)
nvmalloc_non_persist_bench: nvmalloc_non_persist_bench.cc
	$(CXX) nvmalloc_non_persist_bench.cc -o nvmalloc_non_persist_bench  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)

test_del: test_del.cc
	$(CXX) test_del.cc -o test_del  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)

load_file: load_file.cc
	$(CC) load_file.cc -o load_file  $(CFLAGS) $(LIBS) -I$(INCLUDE) $(DEFINES)

read_file: read_file.cc
	$(CC) read_file.cc -o read_file  $(CFLAGS) $(LIBS) -I$(INCLUDE) $(DEFINES)

#nv_remote_chkpt_test:  nv_remote_chkpt_test.cc
#	$(CXX) nv_remote_chkpt_test.cc -o nv_remote_chkpt_test  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)

#nv_rmtckpt_nodirtchk:  nv_rmtckpt_nodirtchk.cc
#	$(CXX) nv_rmtckpt_nodirtchk.cc -o nv_rmtckpt_nodirtchk  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)

nv_test_spinlock:  nv_test_spinlock.cc 
	$(CXX) nv_test_spinlock.cc -o nv_test_spinlock  $(CFLAGS) $(LIBS) -I$(INCLUDE)/jemalloc $(DEFINES)

clean:
	rm -f *.o *.so.0 *.so nv_read_test
	rm -f nvm_rw_test nvm_rw_test nonpersist_alloc
	rm -f test_dirtypgcnt test_dirtypgcpy varname_alloc_test
	rm -f varname_read_test nv_chckpt_test nv_restart_test
	rm -f nv_remote_chkpt_test nv_rmtckpt_nodirtchk
	rm -f nvmalloc_non_persist_bench
	rm -f nonpersist_alloc nvmalloc_c_test hello_world  load_file
