#!/bin/bash
set -x

INSTALL_SYSTEM_LIBS(){
sudo apt-get install -y git
sudo apt-get install -y software-properties-common
sudo apt-get install -y python3-software-properties
sudo apt-get install -y python-software-properties
sudo apt-get install -y unzip
sudo apt-get install -y python-setuptools python-dev build-essential
sudo easy_install pip
sudo apt-get install -y numactl
sudo apt-get install -y libsqlite3-dev
sudo apt-get install -y libnuma-dev
sudo apt-get install -y cmake
sudo apt-get install -y build-essential
sudo apt-get install -y maven
sudo apt-get install -y fio
sudo apt-get install -y libbfio-dev
sudo apt-get install -y libboost-dev
sudo apt-get install -y libboost-thread-dev
sudo apt-get install -y libboost-system-dev
sudo apt-get install -y libboost-program-options-dev
sudo apt-get install -y libconfig-dev
sudo apt-get install -y uthash-dev
sudo apt-get install -y cscope
sudo apt-get install -y msr-tools
sudo apt-get install -y msrtool
sudo pip install psutil
#sudo pip install thrift_compiler
#INSTALL_JAVA
sudo apt-get -y install build-essential
sudo apt-get -y install libssl-dev
sudo apt-get install -y libgflags-dev
sudo apt-get install -y zlib1g-dev
sudo apt-get install -y libbz2-dev
sudo apt-get install -y libevent-dev
sudo apt-get install memcached
}

INSTALL_SPARK(){
sudo service docker stop
sudo apt-get -y remove docker docker.io
sudo rm -rf /var/lib/docker $APPBENCH/docker
mkdir $APPBENCH/docker
sudo apt-get -y install docker docker.io
sudo service docker stop
sudo cp scripts/docker_new.service /lib/systemd/system/docker.service
sudo systemctl daemon-reload
sudo service docker start
sudo docker pull cloudsuite/graph-analytics
sudo docker pull cloudsuite/twitter-dataset-graph
}

INSTALL_CMAKE(){
    wget https://cmake.org/files/v3.7/cmake-3.7.0-rc3.tar.gz
    tar zxvf cmake-3.7.0-rc3.tar.gz
    cd cmake-3.7.0-rc3
    rm -rf CMakeCache*
    ./configure
    ./bootstrap
    make -j16
    make install
}

INSTALL_SYSBENCH() {
        curl -s https://packagecloud.io/install/repositories/akopytov/sysbench/script.deb.sh | sudo bash
        sudo apt -y install sysbench
}

INSTALL_MYSQL() {
        sudo apt-get install mysql-server-5.7

	# change datadir to ssd	
	sudo systemctl stop mysql
	sudo rsync -av /var/lib/mysql $SSD/mysql
	sudo mv /var/lib/mysql /var/lib/mysql.bak
	sed -i '/datadir/d' /etc/mysql/mysql.conf.d/mysqld.cnf | cat -n
	echo 'datadir = $SSD/mysql/mysql' >> /etc/mysql/mysql.conf.d/mysqld.cnf
	echo 'alias /var/lib/mysql/ -> $SSD/mysql,' >> /etc/apparmor.d/tunables/alias
	sudo systemctl restart apparmor
	sudo mkdir /var/lib/mysql/mysql -p
	sudo systemctl start mysql
}

INSTALL_ROCKSDB() {
	cd $APPBENCH/apps
	git clone https://github.com/facebook/rocksdb
	#cp $APPBENCH/apps/db_bench_tool.cc $APPBENCH/apps/rocksdb/tools/
	cd rocksdb
	#mkdir build 
	#cd build
	#rm -rf CMakeCache.txt
	#cmake ..
	DEBUG_LEVEL=0 make shared_lib db_bench -j16
	cp $APPBENCH/apps/rocks-script/run_rocksdb.sh $APPBENCH/apps/rocksdb/run.sh
}

INSTALL_GFLAGS(){
	cd $SHARED_LIBS
	git clone https://github.com/gflags/gflags.git
	cd gflags
	rm -rf CMakeCache.txt
	export CXXFLAGS="-fPIC" && cmake . -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON && make -j16 && sudo make install
}

#Get Other Apps not in out Repo
GETAPPS(){
	mkdir $APPBENCH
	cd $APPBENCH
	git clone https://github.com/SudarsunKannan/leveldb
	mkdir $APPBENCH/apps
	cd $APPBENCH/apps
	git clone https://github.com/SudarsunKannan/fio
	cd $APPBENCH/apps
	git clone https://github.com/memcached/memcached.git
}

CREATE_QEMU_IMAGE() {
	$SCRIPTS/setvars.sh "trusty"
	$SCRIPTS/qemu_create.sh
}

INSTALL_FSOFFLOAD() {
   cd $OFFLOADBASE
   git pull	
   $SCRIPTS/copy_data_to_qemu.sh $OFFLOADBASE/devfs_client root/
}

INSTALL_SYSTEM_LIBS
INSTALL_CMAKE
exit

#TODO:Create only if required. Add a check here

#CREATE_QEMU_IMAGE

#Compile Linux Kernel
cd $OFFLOADBASE
$SCRIPTS/compile_kernel_only.sh
$SCRIPTS/copy_kernel.sh
INSTALL_FSOFFLOAD
exit

# Set variable, setup packages and generate data
#$SCRIPTS/compile_sharedlib.sh
#INSTALL_SPARK
#GETAPPS
#INSTALL_SYSBENCH
#INSTALL_MYSQL
#INSTALL_CMAKE
#INSTALL_GFLAGS
#INSTALL_ROCKSDB

#Changing bandwidth of a NUMA node
#$APPBENCH/install_quartz.sh
#$APPBENCH/throttle.sh
#$APPBENCH/throttle.sh
