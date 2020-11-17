#!/bin/bash
sudo dmesg -c
DEBUG=$1

sudo mount -t crfs -o dbgmask=0x10,vmmode=1,physaddr=0x0,init=64m,num_inodes=4096,jsize=64k,nohugeioremap none /mnt/ram
#sudo mount -t crfs -o physaddr=0x100000000,init=2g none /mnt/ram

sudo chown -R $USER /mnt/ram
