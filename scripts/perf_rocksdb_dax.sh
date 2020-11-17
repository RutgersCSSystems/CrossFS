source scripts/setvars.sh
cd $DEVFSCLIENT
make clean && make && make install
source scripts/setvars.sh
$DEVFSCLIENT/scripts/mountext4dax.sh
cd $ROCKSDB
$ROCKSDB/ext4dax_run.sh "sudarsun-DAX-perf"
