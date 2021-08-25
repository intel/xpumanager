#!/bin/sh

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

if [ ! -d build ]; then
    mkdir build
fi

cd build/

cmake .. -DCMAKE_BUILD_TYPE=Debug 

make