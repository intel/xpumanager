#!/usr/bin/env bash
#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file gen_grpc_py_files.sh
#

WORK=$(dirname "$0")
cd ${WORK}

python3 -m grpc_tools.protoc --python_out=. --grpc_python_out=. -I../daemon core.proto
