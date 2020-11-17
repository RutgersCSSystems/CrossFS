export OFFLOADBASE=$PWD
export BASE=$PWD

#### DEVICE NAME ########
export DEVICE="NVMemu"
#export DEVICE="optane"

export STORAGEPATH="/mnt/ram"
export AUXILLARY="/mnt/pmemdir"

######## DO NOT CHANGE BEYOUND THIS ###########
#Pass the release name
export OS_RELEASE_NAME=$1
export KERN_SRC=$BASE/kernel/linux-4.8.12
export DEVFSCLIENT=$BASE/libfs
export LIBFS=$BASE/libfs
export NVMALLOC_HOME=$LIBFS/nvmlog


#CPU parallelism
export PARA="32"
export VER="4.8.12"
#export VER="4.18.0-2-amd64"

#QEMU
export QEMU_IMG=$BASE
export QEMU_IMG_FILE=$QEMU_IMG/qemu-image-fresh.img
export MOUNT_DIR=$QEMU_IMG/mountdir
export QEMUMEM="36G"
export KERNEL=$BASE/KERNEL

#BENCHMARKS AND LIBS
export LINUX_SCALE_BENCH=$BASE/linux-scalability-benchmark
export APPBENCH=$BASE/appbench
export ROCKSDB=$APPBENCH/RocksDB
export LEVELDB=$APPBENCH/leveldb
export REDIS=$APPBENCH/redis-3.0.0
export FILEBENCH=$APPBENCH/filebench
export SHARED_LIBS=$APPBENCH/shared_libs
export QUARTZ=$BASE/quartz

#SCRIPTS
export SCRIPTS=$BASE/scripts
export INPUTXML=$SCRIPTS/input.xml
export QUARTZSCRIPTS=$SHARED_LIBS/quartz/scripts

export SHARED_DATA=$APPBENCH/shared_data

#export APPPREFIX="numactl --preferred=1"
export APP_PREFIX="numactl --membind=1"

export OUTPUTDIR=$APPBENCH/output
export TEST_TMPDIR=/mnt/pmemdir

#Commands
mkdir $OUTPUTDIR
mkdir $KERNEL
