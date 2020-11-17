SYSCALL_INTERCEPT_DIR := $(abspath $(CURDIR)/syscall_intercept/install/)
INCLUDE=-I$(SYSCALL_INTERCEPT_DIR)/include
SHIMLDFLAGS=-L$(abspath $(SYSCALL_INTERCEPT_DIR)/lib) -Wl,-rpath=$(abspath $(SYSCALL_INTERCEPT_DIR)/lib) -lsyscall_intercept


all: shim.so

shim.so:
	gcc $(INCLUDE) shim.c -lcrfslib $(SHIMLDFLAGS) -fpic -shared -o shim.so

clean:
	rm -rf shim.so syscall_intercept
