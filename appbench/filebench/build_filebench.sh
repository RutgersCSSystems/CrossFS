#!/bin/bash

set -x

libtoolize
aclocal
autoheader
automake --add-missing
autoconf

./configure
make
sudo make install

set +x
