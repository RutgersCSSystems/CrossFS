#!/bin/bash
set -x
mkdir $BASE/mountdir
#Next, mount your image to the directory
sudo mount -o loop $QEMU_IMG_FILE $MOUNT_DIR

