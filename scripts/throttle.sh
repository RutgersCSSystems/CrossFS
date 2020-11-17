#!/bin/bash
set -x
# Simple thermal throttling script for Intel Xeon (Nehalem-based) 
# Authors Sudarsun Kannan (sudarsun@gatech,edu), Vishal Gupta


# Usage:
# sudo $./throttle.sh socket_num throttle_value

# For the exact registers for your platform, please see, Intel software development manual
# and search for  Thermal register values.
# TODO: More generic script for multiple sockets and configurations

#default values
throttle='0x1f0f'
apply='0x2'

#if [ $# -lt 2 ]
  #then
    #echo "   "	
    #echo "Usage ./throttle.sh socket_num throttle_level"
    #echo "   "	
    #echo "Currently this script only supports two sockets, throttle_level values can be 0, 1, 2"
    #echo "TODO: More generic scripts will be updated soon..." 
    #exit	
#fi

#Determines the level of throttling, takes values 0, 1, 2
# Currently in our platform, with LEVEL=1, reduces bandwidth by ~2x-4x, 
# for LEVEL= 2, reduces bandwidth by ~8x-10x
#let LEVEL=$2
# $1= 1 (first socket), $1 = 0 (second sockets), $1 = 2 (for both sockets)
#let SOCKET_NUM=$1

RUNSTREAM() {
  #Compile stream
  cd $OFFLOADBASE/bench/stream && make clean && make
  numactl --membind=0 stream/stream_c.exe &> throttle.out
  numactl --membind=1 stream/stream_c.exe &>> throttle.out
  cd $OFFLOADBASE
}

#Throttle Values. Modify values specific to your platforms using 
#development manual.
SET_THROTTLE_VALUES() {
  if [ $LEVEL == 0 ]
  then #no throttle (disables throttling)
    throttle='0xffff'
    apply='0x0'

  elif [ $LEVEL == 1 ] # reduces bandwidth by 2x
  then #2x
    throttle='0x1f0f'
    apply='0x2'
  elif [ $LEVEL == 2 ]  # reduces bandwidth by 8x
  then #5x
    throttle='0x0f0f'
    apply='0x2'
  fi
}

#Which socket number to throttle. If you have more sockets in the 
# machine, increase the case.
# The NUMA node number and PCI register socket number are reversed in our platform
# So, we socket number for 1 for NUMA node 0, and socket number 0 for NUMA node 1
# TODO: Test in other platforms if this holds true

PERFORM_THROTTLE() {
  if [ $SOCKET_NUM == 1 ] #first socket
  then
    for i in {4..6}
    do
        setpci -s fe:0$i.3 0x84.L=$throttle
        setpci -s fe:0$i.3 0x48.L=$apply
    done
  elif [ $SOCKET_NUM == 0 ] #second socket
  then
    for i in {4..6}
    do
        setpci -s ff:0$i.3 0x84.L=$throttle
        setpci -s ff:0$i.3 0x48.L=$apply
    done
  elif [ $SOCKET_NUM == 2 ] #both sockets
  then
    for i in {4..6}
    do
        setpci -s fe:0$i.3 0x84.L=$throttle
        setpci -s fe:0$i.3 0x48.L=$apply
    done
    for i in {4..6}
    do
        setpci -s ff:0$i.3 0x84.L=$throttle
        setpci -s ff:0$i.3 0x48.L=$apply
    done
  fi
}


INSTALL_THROTTLE_QUARTZ() {
    sudo apt-get install libconfig-dev libmpich-dev uthash-dev
    cd $SHARED_LIBS
    git clone https://github.com/SudarsunKannan/quartz
    $SCRIPTS/install_quartz.sh
}


PERFORM_THROTTLE_QUARTZ() {
     export QUARTZSCRIPTS=$OFFLOADBASE/quartz/scripts
     export APPPREFIX=$QUARTZSCRIPTS/runenv.sh
     # First time to generate the bandwidth model and PCI model of your machine
     $APPPREFIX date

     # We train and exit to a specific value
     #rm -rf /tmp/bandwidth_model

     if [ -f $OFFLOADBASE/bandwidth_model ]; then
         cp $OFFLOADBASE/bandwidth_model /tmp/
     else
         cp /tmp/bandwidth_model $OFFLOADBASE/	
     fi

     if [ -f $OFFLOADBASE/mc_pci_bus ]; then
         cp $OFFLOADBASE/mc_pci_bus /tmp/
     else
         cp /tmp/mc_pci_bus $OFFLOADBASE/
     fi
}

RUNSTREAM
echo " "
echo "-----------------"
echo "BEFORE THROTTLING"
echo "-----------------"
echo "BANDWIDTH NODE 1 and NODE 2 (MB/s)"
grep -r "Copy:" throttle.out | awk '{print $2}'
echo "-----------------"
echo " "


#SET_THROTTLE_VALUES
PERFORM_THROTTLE_QUARTZ

#Wait for sometime for throttling to take effect
sleep 5

RUNSTREAM
echo " "
echo "-----------------"
echo "AFTER THROTTLING"
echo "-----------------"
echo "BANDWIDTH NODE 1 and NODE 2 (MB/s)"
grep -r "Copy:" throttle.out | awk '{print $2}'
echo "-----------------"
echo " "
set +x
