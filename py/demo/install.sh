#!/bin/bash

# create python virtual env

if [ -d xpumvenv ]; then
    echo virtual env exists, will remove the xpumvenv folder
    rm -rf xpumvenv
fi

sudo apt install -y python3-venv

python3 -m venv xpumvenv

# install 3rd party python lib

source ./xpumvenv/bin/activate

python -m pip install --no-cache-dir -r requirements.txt

deactivate

# link firmware flash tool to /usr/local/bin
if ! [ -e /usr/local/bin/GfxFwFPT ]; then
    sudo ln -s $PWD/tool/GfxFwFPT /usr/local/bin/GfxFwFPT
fi

# start daemon
sudo ./start.sh