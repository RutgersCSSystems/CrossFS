#!/bin/bash
#set -x

INSTALL_SYSTEM_LIBS(){
	sudo apt-get install -y cmake libconfig-dev uthash-dev libmpich-dev
	sudo apt-get install -y msr-tools
	sudo apt-get install -y msrtool
	sudo apt-get install -y software-properties-common
	sudo apt-get install -y libmpich-dev
}


BASE=$OFFLOADBASE
cd $OFFLOADBASE
INSTALL_SYSTEM_LIBS

cd $OFFLOADBASE/bench/stream
make

cd $OFFLOADBASE

git clone https://github.com/SudarsunKannan/quartz
cd $OFFLOADBASE/quartz
mkdir build
cd build
rm CMakeCache.txt
cmake ..
make clean all
sudo $OFFLOADBASE/quartz/scripts/setupdev.sh unload
sudo $OFFLOADBASE/quartz/scripts/setupdev.sh load
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
echo 2 | sudo tee /sys/devices/cpu/rdpmc
$SCRIPTS/throttle.sh
$SCRIPTS/throttle.sh
