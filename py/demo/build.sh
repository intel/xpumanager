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

cp ../client/xpumcli build/xpumcli

# core lib
mkdir build/lib
cp ../../lib/libDGMCore.so build/lib/


# supervisor
cp start.sh stop.sh supervisord.conf build

mkdir build/tmp

zip -r demo.zip build