#!/bin/bash
export NVMALLOC_HOME="/users/skannan/ssd/schedsp/nvmalloc"
rm -rf /mnt/pmemdir/*
$NVMALLOC_HOME/test/snappy_test w &
sleep 10
$NVMALLOC_HOME/test/snappy_test r #&
