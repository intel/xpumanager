#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`


rm -rf build
mkdir build

# generate xml using doxygen
doxygen Doxyfile

cd ${WORK_DIR}
# convert xml to rst by doxyrest
DOXYREST_FOLDER="./doxyrest"

cat /etc/os-release | grep 'CentOS' >/dev/null 2>&1
if [ $? -eq 0 ]; then           
    mv $DOXYREST_FOLDER/bin/doxyrest $DOXYREST_FOLDER/bin/doxyrest-ubt
    mv $DOXYREST_FOLDER/bin/doxyrest-rh $DOXYREST_FOLDER/bin/doxyrest
fi

$DOXYREST_FOLDER/bin/doxyrest \
    -c $DOXYREST_FOLDER/share/doxyrest/frame/doxyrest-config.lua \
    -F $DOXYREST_FOLDER/share/doxyrest/frame/cfamily \
    -F $DOXYREST_FOLDER/share/doxyrest/frame/common \
    -o build/rst/index.rst \
    -D SORT_GROUPS_BY="originalIdx" \
    -D INDEX_TITLE="Core Library API" \
    build/doxygen-output/xml/index.xml

cd ${WORK_DIR}

# cp *.rst build/

# cp *.md build/

../rest/gen_grpc_py_files.sh

python3 ../rest/gen_doc.py > build/schema.json

# build rst to html by sphinx
sphinx-build -c sphinxconf/ . build/html