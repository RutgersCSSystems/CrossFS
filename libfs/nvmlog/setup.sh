export NVMALLOC_HOME=`$PWD`

#Install packages
sudo apt-get install libssl-dev
sudo apt-get install libsnappy-dev


#setup tmpfs with size in megabytes
scripts/setuptmpfs.sh 4096
