#!/bin/bash
set -x

#make menuconfig
#make olddefconfig
#Compile the kernel
cd $KERN_SRC
#cd kernel/linux-4.8.12
sudo cp mlfs.config .config

#Compile the kernel with '-j' (denotes parallelism) in sudo mode
sudo make -j$PARA
sudo make modules
sudo make modules_install
sudo make install

y="4.8.12"
	if [[ x$ == x ]];
	then
		echo You have to say a version!
		exit 1
	fi

sudo cp ./arch/x86/boot/bzImage /boot/vmlinuz-$y
sudo cp System.map /boot/System.map-$y
sudo cp .config /boot/config-$y
sudo update-initramfs -c -k $y
