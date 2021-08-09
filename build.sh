#!/bin/bash

rm -rf build
mkdir build

cd build

cmake ..  $@

make

if [ -f "/etc/debian_version" ]; then
    cpack
elif [ -f "/etc/redhat-release" ] || [ -f "/etc/SUSE-release" ]; then
    cpack
fi
#make install