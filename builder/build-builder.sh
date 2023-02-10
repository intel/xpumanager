#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build-builder.sh
#


set -ex

level_zero_version=1.8.5
registry=ccr-registry.caas.intel.com/xpum
image_tag=lz-$level_zero_version-`TZ=PRC date +%Y%m%d`

if [ "$EUID" -ne 0 ]
  then echo "please run as root"
  exit
fi

pushd "$(dirname "$0")"

http_proxy=http://proxy-dmz.intel.com:911
https_proxy=http://proxy-dmz.intel.com:912

centos7_ver=7
centos8_ver=8
suse_ver=15.4
ubuntu_ver_focal=20.04
ubuntu_ver_jammy=22.04

docker pull centos:$centos7_ver
docker pull centos:$centos8_ver
docker pull opensuse/leap:$suse_ver
docker pull ubuntu:$ubuntu_ver_focal
docker pull ubuntu:$ubuntu_ver_jammy

docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg BASE_VERSION=$centos7_ver \
-t $registry/xpum-builder-centos7:$image_tag \
-f Dockerfile.builder-centos7 .. > centos7.log 2>&1 &

docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg BASE_VERSION=$centos8_ver \
-t $registry/xpum-builder-centos8:$image_tag \
-f Dockerfile.builder-centos8 .. > centos8.log 2>&1 &

docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg BASE_VERSION=$suse_ver \
-t $registry/xpum-builder-sles:$image_tag \
-f Dockerfile.builder-sles .. > sles.log 2>&1 &

docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg BASE_VERSION=$ubuntu_ver_focal \
-t $registry/xpum-builder-ubuntu-focal:$image_tag \
-f Dockerfile.builder-ubuntu .. > ubuntu_focal.log 2>&1 &

docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg BASE_VERSION=$ubuntu_ver_jammy \
-t $registry/xpum-builder-ubuntu-jammy:$image_tag \
-f Dockerfile.builder-ubuntu .. > ubuntu_jammy.log 2>&1 &

wait

docker system prune

popd