#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build-builder.sh
#


set -ex

if [ "$EUID" -ne 0 ]
  then echo "please run as root"
  exit
fi

cd "$(dirname "$0")"

if [ ! -d "grpc" ]
then
  git clone --depth 1 -b v1.45.0 https://github.com/grpc/grpc
  pushd grpc
  git submodule update --init
  git -C third_party/zlib pull https://github.com/madler/zlib v1.2.12
  git -C third_party/protobuf pull https://github.com/protocolbuffers/protobuf.git v3.19.4
  popd
else
  echo "Using local grpc repo."
fi

docker build --build-arg http_proxy=http://child-prc.intel.com:913 --build-arg https_proxy=http://child-prc.intel.com:913 -t sh1sdev002.sh.intel.com/xpum-builder-centos -t xpum-builder-centos -f Dockerfile.centos . > centos.log 2>&1 &
docker build --build-arg http_proxy=http://child-prc.intel.com:913 --build-arg https_proxy=http://child-prc.intel.com:913 -t sh1sdev002.sh.intel.com/xpum-builder-ubuntu -t xpum-builder-ubuntu -f Dockerfile.ubuntu . > ubuntu.log 2>&1 &

docker build --build-arg http_proxy=http://child-prc.intel.com:913 --build-arg https_proxy=http://child-prc.intel.com:913 -t sh1sdev002.sh.intel.com/xpum-builder-centos7 -t xpum-builder-centos7 -f Dockerfile.centos7 . > centos7.log 2>&1 &

wait

docker push sh1sdev002.sh.intel.com/xpum-builder-centos
docker push sh1sdev002.sh.intel.com/xpum-builder-ubuntu
