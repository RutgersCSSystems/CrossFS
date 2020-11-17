#!/bin/bash
#set -x
BASENVLIB=/home/hendrix/jan12_link/nvmalloc
DATAORIG=/home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/gallhager_dataset
STORAGE=pmfs
EXEPATH="/home/hendrix/jan12_link/use_case/apps/OpenCV-2.1.0/samples/c/recongnition train"
DATASET=/mnt/$STORAGE/gallhager_dataset
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

if [[ x$1 == x1 ]];
   then
	rm -f /mnt/$STORAGE/*
	$BASENVLIB/test/load_file $DATAORIG 1 1
   else
	cp -r $DATAORIG $DATASET
	cd $DATASET
   fi

mkdir out

sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
sudo sh -c "sync"
sudo sh -c "sync"
sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
/usr/bin/time -v $EXEPATH $DATASET
exit


rm -f /mnt/$STORAGE/out/*
rm -f /mnt/$STORAGE/*.*.bmp
rm -f /mnt/$STORAGE/*.bmp
rm -f /mnt/$STORAGE/*.jpg

if [[ x$1 == x1 ]];
   then
    rm -f /mnt/$STORAGE/*
	$BASENVLIB/test/load_file $DATAORIG 1 1
   else
	cp -r $DATAORIG /mnt/$STORAGE/	
   fi

sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
sudo sh -c "sync"
sudo sh -c "sync"
sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
/usr/bin/time -v $EXEPATH $DATASET 
