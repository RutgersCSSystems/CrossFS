#!/bin/bash
set -x

#Install Quemu
sudo apt-get install qemu
sudo apt-get kernel-package

#Please do not change beyond this

#Now create a disk for your virtual machine 
#for 20GB
qemu-img create $QEMU_IMG_FILE 32g

#Now format your disk with some file system; 
#ext4 in this example
sudo mkfs.ext4 $QEMU_IMG_FILE
sudo chown -R $USER $QEMU_IMG_FILE

#Now create a mount point directory for your image file
mkdir $MOUNT_DIR

#Next, mount your image to the directory
sudo mount -o loop $QEMU_IMG_FILE $MOUNT_DIR

#Install debootstrap
sudo apt-get install debootstrap

#Now get the OS release version using
cat /etc/os-release

#Set family name 
sudo debootstrap --arch amd64 $OS_RELEASE_NAME  $MOUNT_DIR

#Chroot and Now install all your required packages; lets start with vim and build_esstentials.
sudo chroot $MOUNT_DIR && sudo apt-get install vim && sudo apt-get install build-essential && sudo apt-get install ssh
#You are all set. Now unmount your image file from the directory.
exit
