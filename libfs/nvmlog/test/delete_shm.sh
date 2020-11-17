#!/bin/sh
ipcs -m | awk ' $3 == "skannan9" {print $2, $3}' | awk '{ print $1}' | while read i; do ipcrm -m $i; done
ipcs -m | awk ' $3 == "root" {print $2, $3}' | awk '{ print $1}' | while read i; do ipcrm -m $i; done
ipcs -m | awk ' $3 == "hendrix" {print $2, $3}' | awk '{ print $1}' | while read i; do ipcrm -m $i; done
ipcs -m | awk ' $3 == "sudarsun" {print $2, $3}' | awk '{ print $1}' | while read i; do ipcrm -m $i; done


rm -f /tmp/chk*
rm -f /tmp/ramsud/*

#mpd &
