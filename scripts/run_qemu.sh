#!/bin/bash
set -x

$APPBENCH/scripts/umout_qemu.sh
sleep 1
#Launching QEMU

$APPBENCH/scripts/killqemu.sh

sudo qemu-system-x86_64 -kernel $KERNEL/vmlinuz-$VER -hda $QEMU_IMG_FILE -append "root=/dev/sda rw" --curses -m $QEMUMEM -smp cores=16 -cpu host -enable-kvm -device e1000,netdev=net0 -netdev user,id=net0,hostfwd=tcp::5555-:22
exit

#sudo qemu-system-x86_64 -kernel $KERNEL/vmlinuz-$VER -hda $QEMU_IMG_FILE -append "root=/dev/sda rw" --enable-kvm -m $QEMUMEM -smp maxcpus=16  -numa node,nodeid=0,cpus=0-4 -curses -vga std -numa node,nodeid=1,cpus=10-13

sudo qemu-system-x86_64 -kernel $KERNEL/vmlinuz-$VER -hda $QEMU_IMG_FILE -append "root=/dev/sda rw" --enable-kvm -m $QEMUMEM -numa node,nodeid=0,cpus=0-8,mem=2G -numa node,nodeid=1,cpus=9-15,mem=22G -smp sockets=2,cores=2,threads=2,maxcpus=16 -curses #-device e1000,netdev=net0 -netdev user,id=net0,hostfwd=tcp::5555-:22 -redir tcp:4444::4444 -redir tcp:3333::3333 #-net user,hostfwd=tcp:127.0.0.1:2222-:22   #-netdev tap,id=mynet0,ifname=eth0 -device e1000,netdev=mynet0

#-nographic #-display curses
#sudo qemu-system-x86_64 -nographic -kernel $KERNEL/vmlinuz-4.17.0 -hda qemu-image.img -append "root=/dev/sda rw console=ttyAMA0 console=ttyS0" --enable-kvm -m 16G -numa node,nodeid=0,cpus=0-4 -numa node,nodeid=1,cpus=10-13

#sudo qemu-system-x86_64 -kernel $KERNEL/vmlinuz-$VER -hda $QEMU_IMG_FILE -append "root=/dev/sda rw" --enable-kvm -m $QEMUMEM -numa node,nodeid=0,cpus=0-4 -numa node,nodeid=1,cpus=10-13 -nographic -net nic,macaddr=56:44:45:30:31:32,vlan=0 -net tap,script=no,ifname=tap0,vlan=0 &

#--curses

