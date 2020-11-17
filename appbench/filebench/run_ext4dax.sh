#!/bin/bash

FSPATH=/mnt/pmemdir
DEVFSSRC=$DEVFSCLIENT

RESULT_BASE=$BASE/results/apps-$DEVICE/ext4dax
result_dir=$RESULT_BASE/filebench

set -x

# Create output directories
if [ ! -d "$RESULT_BASE" ]; then
        mkdir $RESULT_BASE
fi

if [ ! -d "$result_dir" ]; then
        mkdir $result_dir
fi

if mount | grep $FSPATH > /dev/null; then
    echo "ext4 dax already mounted"
else
	$DEVFSCLIENT/scripts/mountext4dax.sh
fi

#./filebench -f myworkloads/filemicro_seqwrite_ext4.f >> $result_dir/seqwrite.txt
#./filebench -f myworkloads/filemicro_seqwriterand_ext4.f >> $result_dir/seqwriterand.txt
#./filebench -f myworkloads/filemicro_rwrite_ext4.f >> $result_dir/rwrite.txt

$FILEBENCH/filebench -f $FILEBENCH/myworkloads/varmail_ext4.f
exit

sudo ./filebench -f myworkloads/fileserver_ext4.f >> $result_dir/fileserver_$1.txt
sudo ./filebench -f myworkloads/webserver_ext4.f >> $result_dir/webserver_$1.txt
sudo ./filebench -f myworkloads/varmail_ext4.f >> $result_dir/varmail_$1.txt

set +x
