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

proxy=http://child-prc.intel.com:913

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
-t sh1sdev002.sh.intel.com/xpum-builder-centos \
-f Dockerfile.centos . > centos.log 2>&1 &

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
-t sh1sdev002.sh.intel.com/xpum-builder-centos7 \
-f Dockerfile.centos7 . > centos7.log 2>&1 &

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
-t sh1sdev002.sh.intel.com/xpum-builder-sles \
-f Dockerfile.sles . > sles.log 2>&1 &

docker build \
--build-arg http_proxy=$proxy \
--build-arg https_proxy=$proxy \
-t sh1sdev002.sh.intel.com/xpum-builder-ubuntu \
-f Dockerfile.ubuntu . > ubuntu.log 2>&1 &

wait

docker push sh1sdev002.sh.intel.com/xpum-builder-centos
docker push sh1sdev002.sh.intel.com/xpum-builder-centos7
docker push sh1sdev002.sh.intel.com/xpum-builder-ubuntu
docker push sh1sdev002.sh.intel.com/xpum-builder-sles

docker pull sh1sdev002.sh.intel.com/xpum-builder-centos
docker pull sh1sdev002.sh.intel.com/xpum-builder-centos7
docker pull sh1sdev002.sh.intel.com/xpum-builder-ubuntu
docker pull sh1sdev002.sh.intel.com/xpum-builder-sles

docker system prune
