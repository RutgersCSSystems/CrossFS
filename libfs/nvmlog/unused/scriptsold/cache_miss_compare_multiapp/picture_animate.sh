#!/bin/sh
taskset -c 1 animate  -loop 3 -resize 400x400 /home/moblin/multimedia/Pictures/bmp/*.*
taskset -c 1 animate  -loop 3 -resize 400x400 /home/moblin/multimedia/Pictures/jpg/*.*
taskset -c 1 animate -delay 10  -loop 1 -resize 400x400 /home/moblin/multimedia/Pictures/jpg/*.*
#taskset -c 1 animate -delay 100 -loop 1 -resize 400x400 /home/moblin/multimedia/Pictures/jpg/*.*

