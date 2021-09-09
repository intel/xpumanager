#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`


if [ ! -f $WORK_DIR/install/lib/libhwloc.a ]; then
    echo "build hwloc"
    ./autogen.sh --with-pic
    ./configure  --prefix=$WORK_DIR/install --enable-static --disable-shared LDFLAGS="--static" CFLAGS="-fPIC"
    make -j
    make install
fi