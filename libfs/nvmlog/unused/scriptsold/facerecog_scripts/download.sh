#!/bin/sh
while read line; 
	do echo -e "$line\n"; 
	wget $line
	done < $1
