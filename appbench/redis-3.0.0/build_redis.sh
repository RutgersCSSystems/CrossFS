#!/bin/bash

INSTALL_LIB() {
	sudo apt-get -y update
}

COMPILE_REDIS() {
	make MALLOC=libc -j16
}

INSTALL_LIB
COMPILE_REDIS



