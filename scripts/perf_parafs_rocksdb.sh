source scripts/setvars.sh
cd $DEVFSCLIENT
make clean && make && make install
source scripts/setvars.sh
$DEVFSCLIENT/scripts/mountdevfs_sk.sh
cd $ROCKSDB
$ROCKSDB/parafs_perf.sh "parafs_perf"
