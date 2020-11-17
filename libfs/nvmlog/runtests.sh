#!/bin/bash

export NVMALLOC_HOME=`$PWD`

#Clean the tmpfs/object folder
scripts/cleantmpfs.sh

#Write persistent objects
test/persist_alloc

#Write first, and read the persistent objects with length
scripts/cleantmpfs.sh
test/nvm_rw_test w
test/nvm_rw_test r

#Snappy compression example. First generate objects and then read and compress.
scripts/cleantmpfs.sh
test/snappy_test w
test/snappy_test r
