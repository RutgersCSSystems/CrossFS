#!/bin/bash
sudo dmesg -c
DEBUG=$1
#sudo mount -t ramfs -o size=1g ramfs /mnt/ram
#sudo mount -t devfs -o vmmode=1,dbgmask=0,size=16g,dsize=2m,inodeoffsz=2m devfs /mnt/ram 
#sudo mount -t devfs -o vmmode=1,physaddr=0x80000000,init=2m,nohugeioremap none /mnt/ram
#sudo mount -t devfs -o physaddr=0x80000000,init=4m,vmmode=1,dbgmask=$DEBUG,num_inodes=32,jsize=64k,nohugeioremap none /mnt/ram

#sudo mount -t devfs -o dbgmask=0x10,vmmode=1,physaddr=0x0,init=2m,num_inodes=32,jsize=64k,nohugeioremap none /mnt/ram
#sudo mount -t devfs -o physaddr=0x100000000,init=2g none /mnt/ram
sudo mount -t devfs -o physaddr=0x800000000,init=16g none /mnt/ram	#starting at physical ram 32GB, size is 16GB

sudo chown -R $USER /mnt/ram
