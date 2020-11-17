#! /bin/bash

set -x

cd kernel/linux-4.8.12
sudo cp mlfs.config .config
sudo apt-get -y install libdpkg-dev kernel-package

export CONCURRENCY_LEVEL=$PARA
touch REPORTING-BUGS
sudo fakeroot make-kpkg -j$PARA --initrd kernel-image kernel-headers
sudo dpkg -i ../*image*.deb ../*header*.deb


set +x
