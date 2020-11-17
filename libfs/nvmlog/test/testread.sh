#!/bin/bash
export NVMALLOC_HOME="/users/skannan/ssd/schedsp/nvmalloc"
#rm -rf /mnt/pmemdir/*
$NVMALLOC_HOME/test/persist_alloc_read w &\
sleep 5
$NVMALLOC_HOME/test/persist_alloc_read r #&
