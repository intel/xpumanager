#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "please run as root"
  exit
fi

docker images

docker build --build-arg http_proxy=http://child-prc.intel.com:913 --build-arg https_proxy=http://child-prc.intel.com:913 -t sh1sdev002.sh.intel.com/xpum-builder-centos -t xpum-builder-centos -f Dockerfile.centos .
docker build --build-arg http_proxy=http://child-prc.intel.com:913 --build-arg https_proxy=http://child-prc.intel.com:913 -t sh1sdev002.sh.intel.com/xpum-builder-ubuntu -t xpum-builder-ubuntu -f Dockerfile.ubuntu .

docker push sh1sdev002.sh.intel.com/xpum-builder-centos
docker push sh1sdev002.sh.intel.com/xpum-builder-ubuntu
