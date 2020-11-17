#!/bin/bash
set -x

SSD=$HOME/ssd
SSD_DEVICE="/dev/sda"
SSD_PARTITION="/dev/sda4"
USER=ingerido

FORMAT_SSD() {
	mkdir $SSD
	sudo mount $SSD_PARTITION $SSD
	if [ $? -eq 0 ]; then
		sudo chown -R $USER $SSD
		echo OK
	else
		sudo fdisk $SSD_DEVICE
		sudo mkfs.ext4 $SSD_PARTITION
		sudo mount $SSD_PARTITION $SSD
		sudo chown -R $USER $SSD
	fi
	#unlink $LEVELDBHOME
	#mv $LEVELDBHOME $SSD/
	#ln -s $SSD/leveldb-nvm $LEVELDBHOME
}

FORMAT_SSD
