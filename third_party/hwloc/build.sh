#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`
echo "CMAKE_BINARY_DIR=$1"

echo "WORK_DIR=${WORK_DIR}"
cd ${WORK_DIR}
cd ..
cp -r hwloc/  $1/hwloc

cd $1/hwloc
WORK_DIR=`pwd`
echo "new WORK_DIR=${WORK_DIR}"

if [ ! -f $1/hwloc/lib/libhwloc.a ]; then
    echo "build hwloc"
    cd ${WORK_DIR}
    echo "------------------------hwloc autogen---------------------------" 
    ./autogen.sh --with-pic
    cd ${WORK_DIR}
    echo "---------------------hwloc configure [$2]------------------------"

    ccache_opts=
    if command -v ccache &> /dev/null
    then
        ccache_opts=CC='ccache gcc'
    fi
    if [ "$2" = 'Debug' ]; then
      #./configure --prefix=$1/hwloc --enable-debug --disable-shared --disable-libxml2 --disable-libudev LDFLAGS="-lz" CFLAGS="-fPIC" 
      ./configure "$ccache_opts" --prefix=$1/hwloc --enable-static --disable-shared --disable-libxml2 LDFLAGS="--static -lz" CFLAGS="-fPIC $CFLAGS"
    else
      ./configure "$ccache_opts" --prefix=$1/hwloc --enable-static --disable-shared --disable-libxml2 LDFLAGS="--static -lz" CFLAGS="-fPIC $CFLAGS"
    fi
    echo "------------------------hwloc make---------------------------"
    make -j
    echo "------------------------hwloc install---------------------------" 
    #make install cd
    cp -r hwloc/.libs/ lib/
fi

if [ ! -f $1/hwloc/lib/libhwloc.a ]; then
    echo "build hwloc failed! can not find file $1/hwloc/lib/libhwloc.a"
else  
    echo "build hwloc, done!"
fi
