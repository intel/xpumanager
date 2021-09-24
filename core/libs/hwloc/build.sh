#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

echo "WORK_DIR=${WORK_DIR}"
if [ ! -f $WORK_DIR/install/lib/libhwloc.a ]; then
    echo "build hwloc"
    cd ${WORK_DIR}
    echo "------------------------hwloc autogen---------------------------" 
    ./autogen.sh --with-pic
    cd ${WORK_DIR}
    echo "------------------------hwloc configure---------------------------"
    ./configure  --prefix=$WORK_DIR/install --enable-static --disable-shared LDFLAGS="--static -lz" CFLAGS="-fPIC"
    echo "------------------------hwloc make---------------------------"
    make -j 
    echo "------------------------hwloc install---------------------------" 
    make install 
fi

if [ ! -f $WORK_DIR/install/lib/libhwloc.a ]; then
    echo "build hwloc failed! can not find file $WORK_DIR/install/lib/libhwloc.a"
else  
    echo "build hwloc, done!"
fi