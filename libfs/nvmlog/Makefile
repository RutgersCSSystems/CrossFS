LOGGING=/usr/lib64/logging
INCLUDE=-I$(NVMALLOC_HOME) -I$(NVMALLOC_HOME)/pvmobj -I$(NVMALLOC_HOME)/jemalloc -I/usr/include

src_path=$(NVMALLOC_HOME)
alloc_path=$(NVMALLOC_HOME)/allocs
pin_path=$(NVMALLOC_HOME)/pin_interface
pvmobj_path=$(NVMALLOC_HOME)/pvmobj

LIB_PATH := $(NVMALLOC_HOME)
BENCH:= $(NVMALLOC_HOME)/compare_bench
CMU_MALLOC:=$(NVMALLOC_HOME)/compare_bench/cmu_nvram/nvmalloc
LDFLAGS=-ldl
DEBUGFLG=-g

# See source code comments to avoid memory leaks when enabling MALLOC_MAG.
#CPPFLAGS := -DMALLOC_PRODUCTION -DMALLOC_MAG
CPPFLAGS := -fPIC $(DEBUGFLG) #-I$(INCLUDE)/pmem_intel/linux-examples_flex/libpmem -g #-O3
CPPFLAGS:=  $(CPPFLAGS) -lssl -lcrypto -fPIC
CPPFLAGS := $(CPPFLAGS) -DMALLOC_PRODUCTION -fPIC 
CPPFLAGS := $(CPPFLAGS)  -Wno-pointer-arith -Wno-unused-but-set-variable
CPPFLAGS := $(CPPFLAGS)  -Wno-unused-function
CPPFLAGS := $(CPPFLAGS)  -Wno-unused-variable #-fpermissive
CPPFLAGS := $(CPPFLAGS) -cpp 
CPPFLAGS := $(CPPFLAGS) -D_USENVRAM
#CPPFLAGS := $(CPPFLAGS) -D_NVDEBUG
#CPPFLAGS := $(CPPFLAGS) -D_NOCHECKPOINT
#CPPFLAGS := $(CPPFLAGS) -D_USE_CHECKPOINT
#CPPFLAGS := $(CPPFLAGS) -D_ENABLE_RESTART
#CPPFLAGS := $(CPPFLAGS) -D_ENABLE_SWIZZLING
#CPPFLAGS := $(CPPFLAGS) -D_NVRAM_OPTIMIZE

#cache related flags
CPPFLAGS := $(CPPFLAGS) -D_USE_CACHEFLUSH
#CPPFLAGS:= $(CPPFLAGS) -D_LIBPMEMINTEL
#CPPFLAGS := $(CPPFLAGS) -D_USE_HOTPAGE

#allocator usage
#CPPFLAGS := $(CPPFLAGS) -D_USE_JEALOC_PERSISTONLY
#CPPFLAGS:= $(CPPFLAGS) -D_USE_CMU_NVMALLOC
#CPPFLAGS := $(CPPFLAGS) -D_USERANDOM_PROCID
#CPPFLAGS := $(CPPFLAGS) -D_USE_MALLOCLIB

#can be use together or induvidually
#When using transactions, the logging type
#needs to be specified
#CPPFLAGS := $(CPPFLAGS) -D_USE_SHADOWCOPY
#CPPFLAGS := $(CPPFLAGS) -D_USE_TRANSACTION
#CPPFLAGS := $(CPPFLAGS) -D_USE_UNDO_LOG
#CPPFLAGS := $(CPPFLAGS) -D_USE_REDO_LOG
#CPPFLAGS := $(CPPFLAGS) -D_DUMMY_TRANS
#STATS related flags
#CPPFLAGS := $(CPPFLAGS) -D_NVSTATS
#CPPFLAGS := $(CPPFLAGS) -D_FAULT_STATS 
#emulation related flags
CPPFLAGS := $(CPPFLAGS) -D_USE_FAKE_NVMAP
#CPPFLAGS := $(CPPFLAGS) -D_USE_BASIC_MMAP
#CPPFLAGS := $(CPPFLAGS) -D_USEPIN
#checkpoint related flags
#CPPFLAGS := $(CPPFLAGS) -D_ASYNC_RMT_CHKPT
#CPPFLAGS := $(CPPFLAGS) -D_ASYNC_LCL_CHK
#CPPFLAGS := $(CPPFLAGS) -D_RMT_PRECOPY
#CPPFLAGS := $(CPPFLAGS) -D_COMPARE_PAGES 
#CPPFLAGS:= $(CPPFLAGS) -D_ARMCI_CHECKPOINT
#CPPFLAGS:= $(CPPFLAGS) -D_VALIDATE_CHKSM


#Maintains a list of object names for a process
#CPPFLAGS:= $(CPPFLAGS) -D_OBJNAMEMAP
CPPFLAGS:= $(CPPFLAGS) -D_USELOCKS
#CPPFLAGS:= $(CPPFLAGS) -D_ENABLE_INTEL_LOG
#Flags that needs to be cleaned later
#NVFLAGS:= -cpp -D_NOCHECKPOINT $(NVFLAGS)
#NVFLAGS:= -D_VALIDATE_CHKSM -cpp $(NVFLAGS)
#NVFLAGS:= -cpp $(NVFLAGS) 
#NVFLAGS:= -cpp -D_USESCR $(NVFLAGS) -lscrf
#NVFLAGS:= $(NVFLAGS) -D_GTC_STATS
#NVFLAGS:= $(NVFLAGS) -D_COMPARE_PAGES -lsnappy
#NVFLAGS:= -cpp $(NVFLAGS) -D_SYNTHETIC
#NVFLAGS:= -D_USENVRAM -cpp $(NVFLAGS)
#NVFLAGS:= $(NVFLAGS) -D_NVSTATS
#NVFLAGS:= $(NVFLAGS) -D_USE_FAKENO_NVMAP
#NVFLAGS:= $(NVFLAGS) -D_NOCHECKPOINT 
#NVFLAGS:= $(NVFLAGS) -D_ASYNC_RMT_CHKPT
#NVFLAGS:= $(NVFLAGS) -D_RMT_PRECOPY
#NVFLAGS:= $(NVFLAGS) -D_ASYNC_LCL_CHK
CXX=g++
CC=gcc

GNUFLAG :=  -std=gnu99 -std=gnu++11 -fPIC -fopenmp 
CFLAGS := $(DEBUGFLG) $(INCLUDE) -Wall -pipe -fvisibility=hidden \
	  -funroll-loops  -Wno-implicit -Wno-uninitialized \
	  -Wno-unused-function -fPIC -fopenmp #-lpmemlog #-larmci 

STDFLAGS :=-std=gnu++0x 
CPPFLAGS := $(CPPFLAGS) -I$(LOGGING)/include -I$(LOGGING)/include/include -I$(LOGGING)/include/port \
	    -I$(CMU_MALLOC)/include  -I$(BENCH) -I$(BENCH)/compare_bench/c-hashtable

LIBS= -lpthread -L$(LOGGING)/lib64  -lm -lssl \
       -Wl,-z,defs -lpthread -lm -lcrypto -lpthread -L$($(NVMALLOC_HOME))/pmem_nvml/src/debug #-lpmemlog \
       -L$(CMU_MALLOC)/lib  #-lrdpmc 
#      -lpmem \
#      -lnvmalloc #-llogging

all:  SHARED_LIB NVMTEST
test:  SHARED_LIB NVMTEST
benchmark: SHARED_LIB BENCHMARK

JEMALLOC_OBJS= 	$(alloc_path)/jemalloc.o $(alloc_path)/arena.o $(alloc_path)/atomic.o \
        $(alloc_path)/base.o $(alloc_path)/ckh.o $(alloc_path)/ctl.o $(alloc_path)/extent.o \
        $(alloc_path)/hash.o $(alloc_path)/huge.o $(alloc_path)/mb.o \
	$(alloc_path)/mutex.o $(alloc_path)/prof.o $(alloc_path)/quarantine.o \
	$(alloc_path)/rtree.o $(alloc_path)/stats.o $(alloc_path)/tcache.o \
	$(alloc_path)/util.o $(alloc_path)/tsd.o $(alloc_path)/chunk.o \
	$(alloc_path)/bitmap.o $(alloc_path)/chunk_mmap.o $(alloc_path)/chunk_dss.o \
	$(alloc_path)/np_malloc.o #$(src_path)/malloc_hook.o

RBTREE_OBJS= 	$(pvmobj_path)/rbtree.o

NVM_OBJS = $(pvmobj_path)/util_func.o $(pvmobj_path)/cache_flush.o \
	   $(pvmobj_path)/hash_maps.o  $(pvmobj_path)/LogMngr.o\
	   $(pvmobj_path)/nv_map.o \
	   $(pvmobj_path)/nv_transact.o $(pvmobj_path)/nv_stats.o\
	   $(pvmobj_path)/gtthread_spinlocks.o  \
	   $(pvmobj_path)/c_io.o  $(pvmobj_path)/nv_debug.o \
	   #$(src_path)/checkpoint.o 
	   #$(src_path)/nv_rmtckpt.cc 
	   #$(src_path)/armci_checkpoint.o  \
	   
PIN_OBJS = $(pin_path)/pin_mapper.o 	   

BENCHMARK_OBJS = $(BENCH)/c-hashtable/hashtable.o $(BENCH)/c-hashtable/tester.o \
		 $(BENCH)/c-hashtable/hashtable_itr.o $(BENCH)/malloc_bench/nvmalloc_bench.o \
		 $(BENCH)/benchmark.o

$(pvmobj_path)/c_io.o: $(pvmobj_path)/c_io.cc 
	$(CXX) -c $(pvmobj_path)/c_io.cc -o $(pvmobj_path)/c_io.o $(LIBS) $(CPPFLAGS) $(INCLUDE)

$(pvmobj_path)/nv_map.o: $(pvmobj_path)/nv_map.cc 
	$(CXX) -c $(pvmobj_path)/nv_map.cc -o $(pvmobj_path)/nv_map.o $(LIBS) $(CPPFLAGS) $(STDFLAGS) $(INCLUDE)

$(pvmobj_path)/hash_maps.o: $(pvmobj_path)/hash_maps.cc 
	$(CXX) -c $(pvmobj_path)/hash_maps.cc -o $(pvmobj_path)/hash_maps.o $(LIBS) $(CPPFLAGS) $(STDFLAGS)

$(pvmobj_path)/rbtree.o: $(pvmobj_path)/rbtree.cc
	$(CXX) -c $(pvmobj_path)/rbtree.cc -o $(pvmobj_path)/rbtree.o $(LIBS) $(CFLAGS)

$(pvmobj_path)/LogMngr.o: $(pvmobj_path)/LogMngr.cc
	$(CXX) -c $(pvmobj_path)/LogMngr.cc -o $(pvmobj_path)/LogMngr.o $(LIBS) $(CPPFLAGS) $(STDFLAGS)

$(pvmobj_path)/nv_transact.o: $(pvmobj_path)/nv_transact.cc
	$(CXX) -c $(pvmobj_path)/nv_transact.cc -o $(pvmobj_path)/nv_transact.o $(LIBS) $(CPPFLAGS) $(STDFLAGS) $(INCLUDE)

$(pvmobj_path)/nv_stats.o: $(pvmobj_path)/nv_stats.cc
	$(CXX) -c $(pvmobj_path)/nv_stats.cc -o $(pvmobj_path)/nv_stats.o $(LIBS) $(CPPFLAGS) $(STDFLAGS)	

$(pvmobj_path)/cache_flush.o: $(pvmobj_path)/cache_flush.cc
	$(CXX) -c $(pvmobj_path)/cache_flush.cc -o $(pvmobj_path)/cache_flush.o $(LIBS) $(CPPFLAGS) $(STDFLAGS)	

$(pvmobj_path)/nv_debug.o: $(pvmobj_path)/nv_debug.cc
	$(CXX) -c $(pvmobj_path)/nv_debug.cc -o $(pvmobj_path)/nv_debug.o $(LIBS) $(CPPFLAGS) $(STDFLAGS)	

$(pin_path)/pin_mapper.o: $(pin_path)/pin_mapper.cc
	$(CXX) -c $(pin_path)/pin_mapper.cc -o $(pin_path)/pin_mapper.o $(LIBS) $(CPPFLAGS) $(STDFLAGS)	

				
OBJLIST= $(RBTREE_OBJS) $(NVM_OBJS)  $(JEMALLOC_OBJS) #$(PIN_OBJS)
SHARED_LIB: $(RBTREE_OBJS) $(JEMALLOC_OBJS) $(NVM_OBJS)
	#$(CC) -c $(RBTREE_OBJS) -I$(INCLUDE) $(CFLAGS) 
	#$(CC) -c $(JEMALLOC_OBJS) -I$(INCLUDE) $(CFLAGS) $(NVFLAGS)
	#$(CXX) -c $(NVM_OBJS) -I$(INCLUDE) $(CPPFLAGS) $(NVFLAGS)  $(LDFLAGS)
	#ar crf  libnvmchkpt.a $(OBJLIST) $(NVFLAGS)  
	#ar rv  libnvmchkpt.a $(OBJLIST) $(NVFLAGS)
	$(CXX) -shared -fPIC -o libnvmchkpt.so $(OBJLIST) $(NVFLAGS) $(LIBS) $(LDFLAGS) $(CPPFLAGS)
	#$(CXX) -g varname_commit_test.cc -o varname_commit_test $(OBJLIST) -I$(INCLUDE) $(CPPFLAGS) $(NVFLAGS)  $(LIBS)
	#$(CXX) -g varname_commit_test.cc -o varname_commit_test util_func.o -I$(INCLUDE) $(CPPFLAGS) $(LIBS)

BENCHMARK: $(JEMALLOC_OBJS) $(NVM_OBJS) $(BENCHMARK_OBJS)
	$(CXX) -shared -fPIC -o libnvmchkpt.so $(OBJLIST) $(INCLUDE) $(CPPFLAGS) $(NVFLAGS)  $(LIBS)  $(LDFLAGS)
	$(CXX)  $(BENCHMARK_OBJS) -o benchmark $(OBJLIST) $(INCLUDE) $(CPPFLAGS) $(NVFLAGS)  $(LIBS)

NVMTEST:
	cd test && make clean && make && cd ..

clean:
	rm -f *.o *.so.0 *.so *.so* nv_read_test
	rm -f $(alloc_path)/*.o 
	rm -f $(pvmobj_path)/*.o
	rm -f nvmalloc_bench
	rm -f test_dirtypgcnt test_dirtypgcpy
	rm -f ../*.o
	rm -f $(BENCHMARK_OBJS) "benchmark" 
	rm -rf libnvmchkpt.*
	cd test && make clean && cd ..

install:
	sudo cp -r $(NVMALLOC_HOME)/pvmobj/*.h /usr/include/
	sudo cp -r $(NVMALLOC_HOME)/pvmobj/*.h /usr/local/include/
	sudo cp $(NVMALLOC_HOME)/libnvmchkpt.so /usr/local/lib/
	sudo cp $(NVMALLOC_HOME)/libnvmchkpt.so /usr/lib/

uninstall:
	rm -rf libnvmchkpt.*
	rm -rf /usr/lib64/nvmalloc/lib/libnvmchkpt.so*
	sudo rm -rf /usr/local/lib/libnvmchkpt.so*
	sudo rm -rf /usr/lib/libnvmchkpt.so*
	#sudo rm -rf /usr/lib64/libnvmchkpt.so*
	sudo rm -rf /usr/lib/libnvmchkpt.a


