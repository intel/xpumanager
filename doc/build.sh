#!/usr/bin/env bash
#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build.sh
#


WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

cd ${WORK_DIR}

rm -rf build
mkdir build

cd build

cmake .. $@
make -j4