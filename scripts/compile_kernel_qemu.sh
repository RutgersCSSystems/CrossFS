#! /bin/bash

PARA=16

set -x

#cd kernel/kbuild

#make -f Makefile.setup .config
#make -f Makefile.setup
#make -j$PARA
#sudo make modules
#sudo make modules_install
#sudo make install

cd $KERN_SRC
sudo make -j$PARA &> $KERN_SRC/compile.out

sudo cp ./arch/x86/boot/bzImage $KERNEL/vmlinuz-$VER
sudo cp System.map $KERNEL/System.map-$VER
grep -r "error:" $KERN_SRC/compile.out &> $KERN_SRC/errors.out
cat $KERN_SRC/errors.out
set +x
