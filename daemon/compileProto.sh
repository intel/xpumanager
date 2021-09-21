#!/bin/bash

protoc --cpp_out=./ core.proto
protoc --grpc_out=./ --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` core.proto

python -m grpc_tools.protoc --python_out=../py/ --grpc_python_out=../py/ -I. ./core.proto
