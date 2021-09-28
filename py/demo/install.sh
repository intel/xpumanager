#!/bin/bash

# create python virtual env

if [ -d xpumvenv ]; then
    echo virtual env exists;
    exit 0;
fi

python3 -m venv xpumvenv

# install 3rd party python lib

source ./xpumvenv/bin/activate

python -m pip install --no-cache-dir -r requirements.txt

deactivate

# link firmware flash tool to /usr/local/bin
if [ ! -f /usr/local/bin/GfxFwFPT ]; then
    ln -s $PWD/tool/GfxFwFPT /usr/local/bin/GfxFwFPT
fi