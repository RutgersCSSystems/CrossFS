#!/bin/bash

set -x

default=3000000
freq=${1:-$default}
cpucnt=40

#i=0
#while (( $i < $cpucnt ))
#do
        ##echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
        ##echo performance | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
	
        #sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq"
        #sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq"
        #sudo sh -c "echo -n performance > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor"
        ##sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_setspeed"

        #i=$((i+1))
#done

#echo $freq | sudo tee /sys/devices/system/cpu/cpu10/cpufreq/scaling_max_freq
#echo $freq | sudo tee /sys/devices/system/cpu/cpu11/cpufreq/scaling_max_freq
#echo $freq | sudo tee /sys/devices/system/cpu/cpu12/cpufreq/scaling_max_freq
#echo $freq | sudo tee /sys/devices/system/cpu/cpu13/cpufreq/scaling_max_freq

sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu10/cpufreq/scaling_max_freq"
sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu10/cpufreq/scaling_min_freq"
sudo sh -c "echo -n powersave > /sys/devices/system/cpu/cpu10/cpufreq/scaling_governor"

sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu11/cpufreq/scaling_max_freq"
sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu11/cpufreq/scaling_min_freq"
sudo sh -c "echo -n powersave > /sys/devices/system/cpu/cpu11/cpufreq/scaling_governor"

sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu12/cpufreq/scaling_max_freq"
sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu12/cpufreq/scaling_min_freq"
sudo sh -c "echo -n powersave > /sys/devices/system/cpu/cpu12/cpufreq/scaling_governor"

sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu13/cpufreq/scaling_max_freq"
sudo sh -c "echo -n $freq > /sys/devices/system/cpu/cpu13/cpufreq/scaling_min_freq"
sudo sh -c "echo -n powersave > /sys/devices/system/cpu/cpu13/cpufreq/scaling_governor"


set +x
