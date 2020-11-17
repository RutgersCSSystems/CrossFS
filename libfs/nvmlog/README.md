nvm object store library (Under cleanup, and restructuring phase)
---------------------------------------------------------------------

How to build
------------

To compile the library, you need the following libraries

     $ cd nvmalloc
     $ export NVMALLOC_HOME=$PWD
     $ sudo apt-get install libssl-dev

To build the object library, use sudo if required in your machine

    $ make clean
    $ make 
    $ make install	

Setup tmpfs with size in megabytes

    $ scripts/setuptmpfs.sh 4096


How to run (helper script runtests.sh can be also used)
------------------------------------------------------

Clean the tmpfs/object folder 
	
    $ scripts/cleantmpfs.sh

Write first, and read the persistent objects with length

    $ test/nvm_rw_test w
    $ test/nvm_rw_test r


Snappy compression example. First generate objects and then read and compress.


    $ scripts/cleantmpfs.sh
    $ test/snappy_test w
    $ test/snappy_test r


Description
------------

The NVM object store creates objects on a large memory mapped regions
by using a memory allocator and maintains persistent metadata (state)
about the objects, including their location in the mapped regions,
size, object ID, and information about their last commit size.  NVM
object store can be used on any device such as RamDisk, Linux DAX
partition or even a simple Linux directory. 

Internals of nvmalloc 
----------------------

The NVM object store allocator uses `jemalloc' due to its lower
fragmentation and high multi-thread performance. But "jemalloc" and
other modern allocators are complex and maintaining the entire state
of the allocators in the NVM can be complex and expensive. Hence,
nvmalloc creates
simplified replica structures in a log. During application
initialization, a unique gUID is used to create an object store of a
process. Each process object store can have one or more memory mapped
regions which are structured as a virtual memory area (VMA) in the
code. Each VMA also has a per-process ID (MMAPID) on which objects are
created. Objects are called "nvchunks." 


Transactions
------------
nvmalloc uses transactions from Intel's NVML code but is currently
disabled because of significant changes to NVML code. We are in the
process of modifying nvmalloc's interface to suit to NVML's new
transaction code, and the appropriate document will be added soon
about how to use transactions.



To DO
-----

1. Description of different configuration options
2. Test comparisons for POSIX vs. object storage
3. Integration with Intel NVML's changed library abstractions




