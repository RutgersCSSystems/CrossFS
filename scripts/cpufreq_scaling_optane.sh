#!/bin/bash
#set -x

defaultfreq=3000000
freq=1200000
#freq=${1:-$defaultfreq}
defaultpolicy=performance
#policy=${2:-$defaultpolicy}
policy=powersave

# First, scale all CPUs freq to 3.0GHz
i=0
cpucnt=64
while (( $i < $cpucnt ))
do
	echo $defaultfreq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
	echo $defaultfreq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq
	echo $defaultpolicy | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
	i=$((i+1))
done

# Second, scale CPU on socket 1 (16-31, 48-63) to 1.2GHz
i=16
cpucnt=32
while (( $i < $cpucnt ))
do
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq
	echo $policy | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
	i=$((i+1))
done

i=16
cpucnt=32
while (( $i < $cpucnt ))
do
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq
	echo $policy | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
	i=$((i+1))
done

i=48
cpucnt=64
while (( $i < $cpucnt ))
do
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq
	echo $policy | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
	i=$((i+1))
done

i=48
cpucnt=64
while (( $i < $cpucnt ))
do
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
	echo $freq | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq
	echo $policy | sudo tee /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
	i=$((i+1))
done


i=0
cpucnt=64
while (( $i < $cpucnt ))
do
	cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_max_freq
	cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq
	i=$((i+1))
done

#numactl --physcpubind=0 CPUBench/cpubench  50000 --singlethreaded --nodigits
#numactl --membind=1 sysbench --test=cpu --cpu-max-prime=200000 --num-threads=20 run

# Run simple CPU benchmark to make sure the target socket CPU is slowed donw
taskset -c 0 bench/CPUBench/cpubench  50000 --singlethreaded --nodigits
taskset -c 16 bench/CPUBench/cpubench  50000 --singlethreaded --nodigits

#numactl --physcpubind=0 bench/CPUBench/cpubench  50000 --singlethreaded --nodigits
#numactl --physcpubind=1 bench/CPUBench/cpubench  50000 --singlethreaded --nodigits

set +x
