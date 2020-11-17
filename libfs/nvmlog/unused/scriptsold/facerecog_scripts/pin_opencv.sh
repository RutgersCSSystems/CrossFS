#!/bin/bash
#set -x
BASENVLIB=/home/hendrix/jan12_link/nvmalloc
DATAORIG=/home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/gallhager_dataset
STORAGE=pmfs
EXEPATH="/home/hendrix/sda7/home/hendrix/jan12/profilers/pin-2.9-39599-gcc.3.4.6-ia32_intel64-linux/pin -t /home/hendrix/sda7/home/hendrix/jan12/profilers/pin-2.9-39599-gcc.3.4.6-ia32_intel64-linux/source/tools/SimpleExamples/obj-intel64/pinatrace.so -- /home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/recongnition"
DATASET=/mnt/$STORAGE/
PDIR=$PWD

if [[ x$1 == x ]];
   then
      echo You have specify 1 or 0 for using nvm heap interface
      exit 1
   fi

cd /mnt/$STORAGE/
rm -rf  /mnt/$STORAGE/*
rm -rf  /mnt/$STORAGE/
rm -f /mnt/$STORAGE/out/*
rm -f /mnt/$STORAGE/*.bmp
rm -f /mnt/$STORAGE/*.*.bmp
rm -f /mnt/$STORAGE/*.bmp

cp -r /home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/gallhager_dataset /mnt/$STORAGE/
cd gallhager_dataset

/home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/gen_dataset/dir_parse.sh /mnt/$STORAGE/gallhager_dataset/ > train.txt
/home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/gen_dataset/dir_parse.sh /mnt/$STORAGE/gallhager_dataset/ > test.txt

#Delete all shared memory if it exists
/home/hendrix/jan12_link/nvmalloc/scripts/delete_shm.sh

#List all shared memory
ipcs -m

sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
sudo sh -c "sync"
sudo sh -c "sync"
sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
/usr/bin/time -v $EXEPATH train
