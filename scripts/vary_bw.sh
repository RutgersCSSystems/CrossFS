#!/bin/bash
#set -x

cd $OFFLOADBASE
APP=""
TYPE="PARAFS"
SCRIPTS=$OFFLOADBASE/scripts

declare -a bwarr=("500" "2000" "10000")


THROTTLE() {
	source scripts/setvars.sh
	cp $SCRIPTS/nvmemul-throttle-bw.ini $QUARTZ/nvmemul.ini
	$SCRIPTS/throttle.sh
	$SCRIPTS/throttle.sh
}

DISABLE_THROTTLE() {
	source scripts/setvars.sh
	cp $SCRIPTS/nvmemul-nothrottle.ini $QUARTZ/nvmemul.ini
	$SCRIPTS/throttle.sh
}


MOUNTFS() {

	rm -rf  /mnt/ram/*
	sleep 5

	#Enable for PARAFS
	if [ "PARAFS" = "$TYPE" ]
	then
		$DEVFSCLIENT/scripts/mountdevfs_sk.sh
	else
		#Enable for DAX
		$DEVFSCLIENT/scripts/mountext4dax.sh
	fi
}

#MOUNTFS
$SCRIPTS/install_quartz.sh

for bw  in "${bwarr[@]}"
do
	sed -i "/read =/c\read = $bw" $SCRIPTS/nvmemul-throttle-bw.ini
	sed -i "/write =/c\write = $bw" $SCRIPTS/nvmemul-throttle-bw.ini
	THROTTLE

	echo "Running with bandwidth set to $bw"

	#yujie add your code for running script

	sleep 5

done
