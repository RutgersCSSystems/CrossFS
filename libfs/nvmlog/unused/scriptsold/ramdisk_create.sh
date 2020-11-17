#!/bin/sh

 mkdir /tmp/ramdisk;

sudo chmod 777  /tmp/ramdisk

 sudo mount -t tmpfs -o size=$1M tmpfs  /tmp/ramdisk

