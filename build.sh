#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build.sh
#


WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

echo "build distribution package"
cd ${WORK_DIR}
if [ -d build ]; then
    for f in $( ls -a build )
    do
        if [ x"$f" != x"." ] && [ x"$f" != x".." ] \
            && [ x"$f" != x"hwloc" ] \
            && [ x"$f" != x"third_party" ] \
            && [ x"$f" != x"lib" ]; then
            rm -rf build/$f
        fi
    done
    echo "build folder exist."
else
    mkdir build
fi
cd build
cmake ..  $@ 
make -j4

echo "---------Create installation package-----------"
cpack   
