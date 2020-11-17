#! /bin/bash

BASE=$PWD

set -x

# Clean kernel build
cd kernel/linux-4.8.12
sudo make clean

set +x
