#!/bin/bash


if [ -f "/etc/debian_version" ]; then
    rm -rf build-deb
    mkdir build-deb
    cd build-deb
    cmake ..  $@
    make
    cpack
elif [ -f "/etc/redhat-release" ] || [ -f "/etc/SUSE-release" ]; then
    rm -rf build-rpm
    mkdir build-rpm
    cd build-rpm
    cmake ..  $@
    make
    cpack
fi

