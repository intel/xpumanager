#!/bin/sh
WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

rm -rf build demo.zip

mkdir build

cp install.sh xpum-engine readme.md build/

cp ../requirements.txt build/

# python code
mkdir build/py

cp ../*.py build/py/

cp ../client/xpumcli build/py/xpumcli

# CLI wrapper
cp xpumcli build/

# core lib
mkdir build/lib
cp ../../lib/libDGMCore.so build/lib/


# supervisor
cp start.sh stop.sh supervisord.conf build

# create tmp folder
mkdir build/tmp

# firmware flash tool
mkdir build/tool
cp ../../tools/fwUpgrade/GfxFwFPT build/tool

# copy topology config folder
cp -r ../../core/config build/

# copy diag kernel files and media files
cp -r ../../core/kernels ../../core/mediadata build/py

# packaging
zip -r demo.zip build