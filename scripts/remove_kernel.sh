#! /bin/bash

PARA=16

set -x

sudo apt-get purge linux-image-4.8.12
sudo apt-get purge linux-headers-4.8.12
sudo update-grub2

sudo rm kernel/*image*.deb
sudo rm kernel/*header*.deb

set +x
