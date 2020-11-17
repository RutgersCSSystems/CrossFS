#!/bin/bash

# Usage:
# $./mem_throttle.sh socket throttle


if [ $2 == 0 ]
then #no throttle
	#throttle='0xffff'
	#apply='0x0'
	throttle='0x2'
	apply='0x2'
elif [ $2 == 1 ] 
then #2x
	throttle='0x1f0f'
	apply='0x2'
elif [ $2 == 2 ] 
then #5x
	throttle='0x0f0f'
	apply='0x2'
fi


if [ $1 == 0 ] #first socket
	then
	for i in {4..6}
	do
		setpci -s fe:0$i.3 0x84.L=$throttle
		setpci -s fe:0$i.3 0x48.L=$apply
	done
	#for i in {4..6}
	#do
	#	setpci -s ff:0$i.3 0x84.L=$throttle
	#	setpci -s ff:0$i.3 0x48.L=0x0
	#done
elif [ $1 == 1 ] #second socket
	then
	#for i in {4..6}
	#do
	#	setpci -s fe:0$i.3 0x84.L=$throttle
	#	setpci -s fe:0$i.3 0x48.L=0x0
	#done
	for i in {4..6}
	do
		setpci -s ff:0$i.3 0x84.L=$throttle
		setpci -s ff:0$i.3 0x48.L=$apply
	done
elif [ $1 == 2 ] #both
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

sleep 4

echo "validation"
echo "***********************8"

echo "Memory bandwidth when binding to Mem node 0"
numactl --membind=0 /home/hendrix/jan12_link/benchmarks/memory_benchmarks/stream/stream_c.exe 

sleep 2


echo "Memory bandwidth when binding to Mem node 1"
numactl --membind=1 /home/hendrix/jan12_link/benchmarks/memory_benchmarks/stream/stream_c.exe 

