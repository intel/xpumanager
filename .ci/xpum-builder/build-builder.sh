#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build-builder.sh
#


set -ex

level_zero_version=1.8.1
image_tag=lz-$level_zero_version

if [ "$EUID" -ne 0 ]
  then echo "please run as root"
  exit
fi

cd "$(dirname "$0")"

proxy=http://child-prc.intel.com:913

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
--build-arg level_zero_version=$level_zero_version \
-t sh1sdev002.sh.intel.com/xpum-builder-centos:$image_tag \
-f Dockerfile.centos . > centos.log 2>&1 &

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
--build-arg level_zero_version=$level_zero_version \
-t sh1sdev002.sh.intel.com/xpum-builder-centos7:$image_tag \
-f Dockerfile.centos7 . > centos7.log 2>&1 &

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
--build-arg level_zero_version=$level_zero_version \
-t sh1sdev002.sh.intel.com/xpum-builder-sles:$image_tag \
-f Dockerfile.sles . > sles.log 2>&1 &

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
--build-arg level_zero_version=$level_zero_version \
-t sh1sdev002.sh.intel.com/xpum-builder-ubuntu:$image_tag \
-f Dockerfile.ubuntu . > ubuntu.log 2>&1 &

wait

docker push sh1sdev002.sh.intel.com/xpum-builder-centos:$image_tag
docker push sh1sdev002.sh.intel.com/xpum-builder-centos7:$image_tag
docker push sh1sdev002.sh.intel.com/xpum-builder-ubuntu:$image_tag
docker push sh1sdev002.sh.intel.com/xpum-builder-sles:$image_tag

docker pull sh1sdev002.sh.intel.com/xpum-builder-centos:$image_tag
docker pull sh1sdev002.sh.intel.com/xpum-builder-centos7:$image_tag
docker pull sh1sdev002.sh.intel.com/xpum-builder-ubuntu:$image_tag
docker pull sh1sdev002.sh.intel.com/xpum-builder-sles:$image_tag

docker system prune
