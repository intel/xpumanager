#!/usr/bin/env bash
#
# Copyright (C) 2021-2024 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build.sh
#

set -e

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
ccache_opts=
if command -v ccache &> /dev/null
then
    ccache_opts="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
fi
prefix_default="-DCMAKE_INSTALL_PREFIX=/usr"
cmake .. $ccache_opts $prefix_default $@
make -j4

echo "---------Create installation package-----------"
cpack   