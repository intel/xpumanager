#!/bin/bash

WORK=$(dirname "$0")
cd ${WORK}

python3 -m grpc_tools.protoc --python_out=. --grpc_python_out=. -I../daemon core.proto
