#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build.sh
#


WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

cd ${WORK_DIR}

rm -rf build
mkdir build

# generate xml using doxygen
doxygen Doxyfile

# convert xml to rst by doxyrest
DOXYREST_FOLDER="./doxyrest"

DOXYREST_PATH=$DOXYREST_FOLDER/bin/doxyrest

cat /etc/os-release | grep 'CentOS' >/dev/null 2>&1
if [ $? -eq 0 ]; then           
    DOXYREST_PATH=$DOXYREST_FOLDER/bin/doxyrest-rh
fi

$DOXYREST_PATH \
    -c $DOXYREST_FOLDER/share/doxyrest/frame/doxyrest-config.lua \
    -F $DOXYREST_FOLDER/share/doxyrest/frame/cfamily \
    -F $DOXYREST_FOLDER/share/doxyrest/frame/common \
    -o build/rst/index.rst \
    -D SORT_GROUPS_BY="originalIdx" \
    -D INDEX_TITLE="Core Library API" \
    build/doxygen-output/xml/index.xml

cd ${WORK_DIR}

# cp *.rst build/

cp ../*.md build/

mkdir build/doc

cp -r img build/doc/img

../rest/gen_grpc_py_files.sh

python3 ../rest/gen_doc.py > build/schema.yml

# build rst to html by sphinx
sphinx-build -c sphinxconf/ . build/html