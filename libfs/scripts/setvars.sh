#!/bin/bash
export DEVFSSRC=$PWD
export STRATASRC=../strata
export DEVFSSCRIPT=$DEVFSSRC/scripts
export FSPATH=/mnt/ram
export EXT4PATH=/mnt/pmemdir

#Create FS mountpoint
sudo mkdir $FSPATH
sudo chown -R $USER $FSPATH

sudo mkdir $EXT4PATH
sudo chown -R $USER $EXT4PATH

#Set the number of files to 1M
#sudo ulimit -u 10000000
sudo bash -c "ulimit -u 10000000"
sudo sysctl -w fs.file-max=10000000

