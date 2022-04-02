#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file compileProto.sh
#


protoc --cpp_out=./ core.proto
protoc --grpc_out=./ --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) core.proto

python -m grpc_tools.protoc --python_out=../rest/stub/ --grpc_python_out=../rest/stub/ -I. ./core.proto
