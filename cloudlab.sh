#!/bin/bash

HOMEDIR=$HOME
SCRIPTS=$PWD

INSTALL_SYSTEM_LIBS(){
	sudo apt-get update
	sudo apt-get install -y git
	sudo apt-get install -y kernel-package
	sudo apt-get install -y software-properties-common
	sudo apt-get install -y python3-software-properties
	sudo apt-get install -y python-software-properties
	sudo apt-get install -y unzip
	sudo apt-get install -y python-setuptools python-dev build-essential
	sudo easy_install pip
	sudo apt-get install -y numactl
	sudo apt-get install -y libsqlite3-dev
	sudo apt-get install -y libnuma-dev
	sudo apt-get install -y libkrb5-dev
	sudo apt-get install -y libsasl2-dev
	sudo apt-get install -y cmake
	sudo apt-get install -y maven
	sudo apt-get install -y mosh
	sudo apt-get install -y libmm-dev
}

# Install required libraries for CrossFS
INSTALL_SYSTEM_LIBS
