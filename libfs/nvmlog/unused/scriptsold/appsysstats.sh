#!/bin/bash

echo "Voluntary context switches"
grep -r "Voluntary context switches" $1 | awk '{print $4}'


echo "Involuntary context switches"
grep -r "Involuntary context switches:" $1 | awk '{print $4}'

echo "User time"
grep -r "User time" $1 | awk '{print $4}'

echo "System time"
grep -r "System time" $1 | awk '{print $4}'

echo "CPU this job got"
grep -r "CPU this job got" $1 | awk '{print $i7}'

echo "Maximum resident set size"
grep -r "Maximum resident set size " $1 | awk '{print $6}'

echo "Minor Faults"
grep -r "Minor " $1 | awk '{print $7}'

echo "Major Faults"
grep -r "Major " $1 | awk '{print $6}'


echo "File system inputs"
grep -r "File system inputs" $1 | awk '{print $4}'


echo "File system outputs"
grep -r "File system outputs" $1 | awk '{print $4}'
