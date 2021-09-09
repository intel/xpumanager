#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

echo "WORK_DIR=${WORK_DIR}"
if [ ! -f $WORK_DIR/install/lib/libhwloc.a ]; then
    echo "build hwloc"
    cd ${WORK_DIR}
    echo "------------------------hwloc autogen---------------------------" > hwloc.log  2>&1
    ./autogen.sh --with-pic >> hwloc.log  2>&1
    cd ${WORK_DIR}
    echo "------------------------hwloc configure---------------------------" >> hwloc.log  2>&1
    ./configure  --prefix=$WORK_DIR/install --enable-static --disable-shared LDFLAGS="--static" CFLAGS="-fPIC" >> hwloc.log  2>&1
    echo "------------------------hwloc make---------------------------" >> hwloc.log  2>&1
    make -j >> hwloc.log  2>&1
    echo "------------------------hwloc install---------------------------" >> hwloc.log  2>&1
    make install >> hwloc.log  2>&1
    echo "------------------------hwloc end---------------------------" >> hwloc.log  2>&1
fi