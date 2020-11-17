CrossFS: A Cross-layered Direct-Access File System
=================================================================
CrossFS is a direct-access cross-layered file system with support for concurrency. CrossFS comprises of a user-level component (libfs) and device-level component emulated using kernel (crfs) with the OS component spread across crfs and some parts of the VFS.

The user-level component intercepts I/O calls, converts them to device-specifc nvme-based commands (VFIO), and sends request and response through shared DMA-able memory regions. The user-level component also creates per-file FD-queues to dispatch requests across queues. 

The firmware-level component (fs/crfs) provides a firmware-level file system that processes the request in parallel when possible. The firmware file system (ffs) also implements a scheduler to prioritize blocking operations.

CrossFS has several components and depdent libraries (see the code structure below).
### CrossFS directory structure

    ├── libfs                          # userspace library (LibFS)
    ├── libfs/scripts                  # scripts to mount CrossFS and microbenchmark scripts
    ├── libfs/benchmark                # microbenchmark executables
    ├── kernel/linux-4.8.12            # Linux kernel
    ├── kernel/linux-4.8.12/fs/crfs    # emulated device firmware file system (FirmFS)
    ├── appbench                       # application workloads
    ├── LICENSE
    └── README.md

The crfslibio.c is the heart of the LibFS responsible for interacting with the FirmwareFS, create file descriptor-based I/O queues, and interval tree operations.
In the FirmwareFS (kernel/linux-4.8.12/fs/crfs), the crfs.c is responsible for fetching the request from I/O queues and processing them. Other files in crfs include file system management as well as for scalability (crfs_scalability.c) and scheduling (crfs_scheduler.c). Other files are for block, metadata, superblock, journaling management, and security similar to other file systems.

### CrossFS kernel-bypass implementation technique

In contrast to prior research like DevFS[FAST' 18] that porting the entire applications to the Linux Kernel, CrossFS takes a more generic approach. To reduce system call cost for an emulated device firmware-level file system (FirmFS), LibFS registers an FD-queue in the FirmFS by mapping the same physical pages also into the Linux kernel. The LibFS maintains a submission head indicating which FD-queue entry is available and FirmFS maintains a completion head indicating which FD-queue entry has completed. By atomically inserting an request to the FD-queue by LibFS, system calls are not needed for I/O requets.

### CrossFS Hardware and OS Environment

CrossFS can be run on both emulated NVM and real Optane-based NVM platform by reserving a region of physical memory (similar to DAX), mounting, and use the region for storing filesystem meta-data and data. 

To enable users to use generally available machine, this documentation will mainly focus on emulated (DAX-based) NVM platform. Users can create a cloudlab instance to run our code (see details below). Unfortunately, the NVM optane (DC memory) used in this study are loaned and requires institutional permissions and with specific OS-version. We will remove the OS version limitations during the final code release.

We currently support Ubuntu-based 16.04 kernels and all pacakge installation scripts use debian. While our changes would also run in 18.04 based Ubuntu kernel, due recent change in one of packages (Shim), we can no longer confirm this. Please see Shim discussion below.

#### Getting and Using Ubuntu 16.04 kernel
We encourage users to use NSF CloudLab (see for details). Use the image type "UWMadison744-F18".


#### CloudLab - Partitioning a SSD and downloading the code.
If you are using CrossFS in CloudLab, the root partition is only 16GB for some profiles.
First setup the CloudLab node with SSD and install all the required libraries.

```
lsblk
```

You should see the following indicating the root partition size is very small:
```
NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
sda      8:0    0 447.1G  0 disk 
├─sda1   8:1    0    16G  0 part /
├─sda2   8:2    0     3G  0 part 
├─sda3   8:3    0     3G  0 part [SWAP]
└─sda4   8:4    0 425.1G  0 part 
sdb      8:16   0   1.1T  0 disk 
```

We suggest using an Ubuntu 16.04 kernel in the CloudLab. You can use the following profile 
to create a CloudLab node "UWMadison744-F18"

```
git clone https://github.com/ingerido/CloudlabScripts
cd CloudlabScripts
./cloudlab_setup.sh
```
Since /dev/sda4 is already partitioned, you could just press "q" in fdisk. Otherwise, if you are using another parition, please use the following steps.
Type 'n' and press enter for all other prompts
```
Command (m for help): n
Partition type
   p   primary (0 primary, 0 extended, 4 free)
   e   extended (container for logical partitions)
Select (default p):
....
Last sector, +sectors or +size{K,M,G,T,P} (2048-937703087, default 937703087):
....
Created a new partition 1 of type 'Linux' and of size 447.1 GiB.
```

Now, time to save the partition. When prompted, enter 'w'. Your changes will be persisted.
```
Command (m for help): w
The partition table has been altered.
Calling ioctl() to re-read partition table.
Syncing disks.
.....
```
This will be followed by a script that installs all the required libraries. Please wait patiently 
to complete. Ath the end of it, you will see a mounted SSD partition.


And then after finish, type:

```
findmnt
```

You will see:

```
/users/$Your_User_Name/ssd                 /dev/sda4   ext4        rw,relatime,data=ordered
```

When compiling our Linux kernel, you will need to reboot the machine. Hence we suggest you also modify /etc/fstab to make sure the ssd partition will be mounted automatically during system boot.

```
sudo vim /etc/fstab

/dev/sda4       /users/$Your_User_Name/ssd     ext4    defaults        0       0
```

#### Changing Max open files 
sudo vim /etc/security/limits.conf

```
root             soft    nofile          1000000
root             hard    nofile          1000000
$Your_User_Name  soft    nofile          1000000
$Your_User_Name  hard    nofile          1000000
```
In addition,
```
sudo sysctl -w fs.file-max=1000000
```
### Compiling CrossFS and other kernels

#### Get CrossFS source code on Github
```
cd ~/ssd
git clone https://github.com/RutgersCSHeteroLab/CrossFS
```

#### Install required libraries for CrossFS
```
cd CrossFS
./cloudlab.sh
```
NOTE: If you are prompted during Ubuntu package installation, please hit enter and all the package installation to complete.

#### Setting environmental variables
```
source scripts/setvars.sh
```

#### Setup for emulated NVM machine in CloudLab

#### Compile kernel and install

First compile the kernel. This would compile both CrossFS firmware-level driver and DAX.

```
$BASE/scripts/compile_kernel_install.sh (Only run this when first time install kernel)
$BASE/scripts/compile_kernel.sh (Run this when kernel is already installed)
```

Now, time to update the grub reserve NVM in the emulated region.

```
sudo vim /etc/default/grub
```

Add the following to GRUB_CMDLINE_LINUX variable in /etc/default/grub
```
GRUB_CMDLINE_LINUX="memmap=80G\$60G"
```
This will reserve contiguous 60GB memory starting from 80GB). This will ideally
use one socket (numa node) of the system. If your system has a lower memory
capacity, please change the values accordingly.

If this does not work, then try the following. This might add the memmap configuration for all kernels. If not, just manually modify the boot parameter.
```
GRUB_CMDLINE_LINUX_DEFAULT="memmap=80G\$60G"
sudo update-grub
sudo reboot
```

After the reboot, the reserved region of memory would disappear from the OS.

### Compiling Userspace Libraries

#### Build and install user-space library (LibFS):
```
cd ssd/CrossFS
source scripts/setvars.sh
cd $LIBFS
source scripts/setvars.sh
./scripts/compile_libfs.sh
```

Next, time to build SHIM library derived and enahnced from Strata [SOSP '17].
This library is responsible for intercepting POSIX I/O operations and directing them to the CrossFS client 
library for converting them to CrossFS commands.

Please note that SHIM library is currently working in 16.04 and is not support 18.04 kernels due to their recent update 
(https://github.com/pmem/syscall_intercept/issues/106). We are in the process of fixing it by ourself.

```
cd $LIBFS/libshim
./makeshim.sh
cd ..
```

#### Mounting CrossFS

```
cd $BASE/libfs
source scripts/setvars.sh
./scripts/mount_crossfs.sh //(Mount CrossFS with 60GB (Starting at 80GB))
```
If successful, you will find a device-level FS mounted as follows after executing the following command
```
mount
```
none on /mnt/ram type crfs (rw,relatime,...)


NOTE: This is a complex architecture with threads in host and kernel-emulated device. If something hangs, 
try running the following and killing the process and restarting. We are doing more testing and development.
```
cd $LIBFS
benchmark/crfs_exit
```

### Running Microbenchmarks

#### Reader-writer Scalability Test 
To run the scalability test, use the following. The test runs concurrent readers (1-16) 
and writers (4) that concurrently update and access a shared file. (Please wait patiently 
as it takes around 2 minutes to complete).

```
cd $LIBFS
./scripts/run_scalability_crfs.sh 
```

#### Scheduler Performance Impact Test 
To test the CrossFS urgency-aware (read-prioritized) scheduler against default round-robin scheduler, 
run the following script. The output is written to $BASE/paper-results/microbench-NVMemu/parafs-noioctl/scheduler  
under read-prioritized (rp) and round-round (rr) folders.
```
cd $LIBFS
scripts/run_scheduler_crfs.sh
```
#### Fsync tests
The tests run a stream of fsync operations across multiple threads forcing immediate commit from file descriptor queue to firmware-level file system.
The output is written to the fsync folder ...microbench-NVMemu/parafs-noioctl/fsync/$threadcount/output.txt
```
cd $LIBFS
./scripts/run_fsync_crfs.sh
```

###  Running Filebench
First, we need to apply a kernel patch changing some configurations for FirmFS in our kernel.
```
cd $BASE
git apply --whitespace=nowarn filebench.patch
```

Then compile the CrossFS kernel
```
$BASE/scripts/compile_kernel.sh
sudo reboot
```

After reboot the machine, mount CrossFS (see section Mounting CrossFS), and then build Filebench
```
cd $BASE/appbench/filebench
./build_filebench.sh
```

Run Filebench with CrossFS
```
./run_crfs.sh
```

When finally you finish running Filebench, please revert the configurations in FirmFS if you want to run other workloads.
```
cd $BASE
git apply -R filebench.patch
```

###  Running Applications

### Running Redis with CrossFS

First, we need to apply a kernel patch changing some configurations for both LibFS and FirmFS in our kernel.
```
cd $BASE
git apply --whitespace=nowarn redis.patch
```

Then compile the CrossFS kernel
```
$BASE/scripts/compile_kernel.sh
sudo reboot
```

After reboot the machine, mount CrossFS (see section Mounting CrossFS), and then rebuild LibFS
```
cd $BASE/libfs
./scripts/compile_libfs.sh
```

#### Compile and Setup Redis
```
cd $BASE
source scripts/setvars.sh
```

First compile the redis-3.0.0
```
cd $BASE/appbench/redis-3.0.0  
./build_redis.sh
```
Copy redis.conf file  
```
sudo cp redis.conf /etc/  
```

#### Run Redis benchmark 
Runs one instance of redis and redis-benchmark. The output is written to 
$BASE/results/apps-NVMemu/parafs-noioctl/redis/NUM-inst with values for each instance count.
Note that after the benchmark is finished, the redis server is killed to terminate the redis server.
```
./crfs_run_small.sh
```

#### Run Redis benchmark - multiple instances (long running)
Runs for multiple redis instances.
```
./crfs_run_multi.sh
```

When finally you finish running Redis, please revert the configurations in LibFS and FirmFS and recompile if you want to run other workloads.
```
cd $BASE
git apply -R redis.patch
cd $BASE/libfs
./scripts/compile_libfs.sh
```


### Compile and Run RocksDB with CrossFS
First, we need to apply a kernel patch changing some configurations for FirmFS in our kernel.
```
cd $BASE
git apply --whitespace=nowarn rocksdb.patch
```

Then compile the CrossFS kernel
```
$BASE/scripts/compile_kernel.sh
sudo reboot
```

After rebooting, compile and build RocksDB
```
cd $BASE/appbench/RocksDB
 ./build_rocksdb.sh
```

#### Run RocksDB benchmark

Run a short 4-thread RocksDB run with CrossFS
```
./crfs_run_small.sh
```

Run a full RocksDB run with CrossFS when varying the application thread count.
The output is written to $BASE/paper-results/apps-NVMemu/parafs-noioctl/rocksdb
```
./crfs_run.sh
```

When finally you finish running RocksDB, please revert the configurations in FirmFS if you want to run other workloads.
```
cd $BASE
git apply -R rocksdb.patch
```


### To enable NVM-based FD-queues
To enable NVM-based FD-queues, we use a in-home NVM logging library that performs persistent allocations of the 
queues.
```
cd $LIBFS
```
In the Makefile, enable the following "FLAGS+=-D_NVMFDQ" and run 
the benchmark. We will use a memory-based filesystem to store the per-FD command queues and the buffers.

To enable this, either use a DAX device mounted to */mnt/pmemdir* or to quickly try it out, allocate some 
*tmpfs* in */mnt/pmemdir*, which will be used to allocate and map FD-queues. 
```
$NVMALLOC_HOME/scripts/setuptmpfs.sh 8192
```
The example shows using 8GB of *tmpfs* for storing NVM-based FD-queues.  You can reduce or increase the size of 
the NVM queue space. If successful, you will see /mnt/pmedir mounted as *tmpfs*
Now clean the libfs and run the benchmark
```
cd $LIBFS
scripts/compile_libfs.sh
./scripts/run_scalability_crfs.sh
```

## Running benchmarks and applications with ext4-DAX
------------------------
If you were running other file systems such as CrossFS, please restart the machine

### Compiling Ext4-DAX

```
cd CrossFS //the base folder
```

#### Setting environmental variables
```
source scripts/setvars.sh
```

#### Setup for emulated NVM machine in CloudLab
```
cd $BASE/scripts/compile_kernel_install.sh (Only run this when first time install kernel)
cd $BASE/scripts/compile_kernel.sh (Run this when kernel is already installed)
```
Now, time to update the grub reserve NVM in the emulated region.

Add "memmap=60G!80G" to /etc/default/grub in GRUB_CMDLIN_LINUX
```
sudo update-grub
sudo reboot
```

#### Mounting Filesystem

##### Ext4-DAX
```
cd $BASE/libfs
source scripts/setvars.sh
./scripts/mountext4dax.sh
```

### Running Microbenchmarks

#### Reader-writer Scalability Test 
```
cd $BASE/libfs
./scripts/run_scalability_ext4dax.sh
```
#### Fsync tests
```
cd $BASE/libfs
./scripts/run_fsync_ext4dax.sh
```

###  Running Filebench
First, we need to apply a kernel patch changing some configurations for FirmFS in our kernel.
```
cd $BASE
source scripts/setvars.sh
```

Build Filebench
```
cd $BASE/appbench/filebench
./build_filebench.sh
```

Run Filebench
```
./run_ext4dax.sh
```

###  Running Applications

#### Running Redis with CrossFS
```
cd aecross
source scripts/setvars.sh
```
#### Compile and Setup Redis
First compile the redis-3.0.0
```
cd $BASE/appbench/redis-3.0.0  
make  
sudo make install  
```

Copy redis.conf file  

```
 sudo cp redis.conf /etc/  
```
#### Run Redis benchmark
```
./ext4dax_run_multi.sh
```

### Compile RocksDB
```
 cd $BASE/appbench/RocksDB
 ./build_rocksdb.sh
```

#### Run RocksDB benchmark
```
./ext4dax_run.sh
```

## Limitations
1. FirmFS is currently implemented in the OS kernel as a device driver.
2. Crash and recovery are not fully tested in a bare-metal with this version of CrossFS.
3. Benchmarks are not fully tested in all configurations. Working configurations are described in this README.











