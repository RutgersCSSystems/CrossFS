CC = gcc
# The compiling order matters
OBJS = syscalldb.o shim_init.o \
	   shim_sys_fs.o \
	   shim_syscalls.o \
	   syscall_trace.o \
	   shim_table.o

HEADERS = $(wildcard *.h)

CUR_DIR = $(shell pwd)
TRACE=

CFLAGS = -fno-strict-aliasing -Wall -fno-omit-frame-pointer #-Wno-initializer-overrides  
#ASFLAGS = -Wa,--noexecstack -x assembler-with-cpp 
LDFLAGS =  -lm -ldevfslib

#DEBUG = -g -O0
DEBUG = -O3 -g
libshim.so: $(OBJS)
	$(CC) -shared $(DEBUG) -o $@ $^ -ldl $(LDFLAGS) $(TRACE)

shim_table.o: shim_table.c
	gcc -g -c -Wall -fPIC $(CFLAGS) $< -o $@ $(LDFLAGS) $(TRACE)

%.o: %.c $(HEADERS)
	@#$(CC) -E -Werror -fPIC $(CFLAGS) $<  -o $(patsubst %.o, %.i, $@)
	$(CC) -g -c -Wall -fPIC $(CFLAGS) $< -o $@ $(LDFLAGS) $(TRACE)

%.o: %.S $(HEADERS)
	$(AS) $(ASFLAGS) -g -c $< -o $@

clean:
	rm -rf libshim.so $(OBJS) *.i
