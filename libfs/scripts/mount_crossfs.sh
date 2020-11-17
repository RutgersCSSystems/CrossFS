#!/bin/bash
sudo dmesg -c
DEBUG=$1
sudo mkdir $STORAGEPATH
sudo mount -t crfs -o physaddr=0x1400000000,init=60g none /mnt/ram	#starting at physical ram 32GB, size is 16GB
sudo chown -R $USER $STORAGEPATH
