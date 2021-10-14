#!/bin/bash

rm -rf build

mkdir build

# generate xml using doxygen
doxygen Doxyfile

# convert xml to rst by doxyrest
DOXYREST_FOLDER="./doxyrest"

$DOXYREST_FOLDER/bin/doxyrest \
    -c $DOXYREST_FOLDER/share/doxyrest/frame/doxyrest-config.lua \
    -F $DOXYREST_FOLDER/share/doxyrest/frame/cfamily \
    -F $DOXYREST_FOLDER/share/doxyrest/frame/common \
    -o build/rst/index.rst \
    build/doxygen-output/xml/index.xml

# build rst to html by sphinx
sphinx-build -c sphinxconf/ build/rst/ build/html