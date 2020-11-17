#!/bin/bash -x
set -x
PROC=30
cd $KERN_SRC
sudo CC=/usr/lib/ccache/bin/gcc make -j$PROC &>compile.out
sudo grep -r "error:" compile.out &> errors.out
sudo grep -r "undefined:" compile.out &> errors.out
sudo CC=/usr/lib/ccache/bin/gcc make bzImage -j$PROC &>>compile.out
sudo grep -r "error:" compile.out &> errors.out
sudo grep -r "undefined:" compile.out &> errors.out
sudo CC=/usr/lib/ccache/bin/gcc make  modules -j$PROC &>>compile.out
sudo CC=/usr/lib/ccache/bin/gcc make  modules_install -j$PROC &>> compile.out
sudo CC=/usr/lib/ccache/bin/gcc make install &>> compile.out

y="4.8.12"
   if [[ x$ == x ]];
  then
      echo You have to say a version!
      exit 1
   fi

sudo cp ./arch/x86/boot/bzImage /boot/vmlinuz-$y
sudo cp System.map /boot/System.map-$y
sudo cp .config /boot/config-$y
sudo rm -rf /boot/initrd.img-$y
sudo update-initramfs -c -k $y
#echo Now edit menu.lst or run /sbin/update-grub

grep -r "warning:" compile.out &> warnings.out
grep -r "error:" compile.out &> errors.out
grep -r "undefined:" compile.out &> errors.out
set +x
