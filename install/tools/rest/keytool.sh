#!/usr/bin/env bash
#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file keytool.sh
#


WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`
echo "WORK_DIR: ${WORK_DIR}"
OWNER=xpum
GROUP=root
for i in "$@"; do
  case $i in
    -o=*|--owner=*)
      OWNER="${i#*=}"
      shift # past argument=value
      ;;
    -g=*|--group=*)
      GROUP="${i#*=}"
      shift # past argument=value
      ;;
    *)
      # unknown option
      ;;
  esac
done
umask 007
openssl req -x509 -sha512 -newkey rsa:3072 -nodes -keyout rest/conf/key.pem -out rest/conf/cert.pem -days 548 -subj "/ST=Unknown/O=XPUM"
echo "Set key file owner:group as ${OWNER}:${GROUP}"
chown ${OWNER}:${GROUP} rest/conf/key.pem
chown ${OWNER}:${GROUP} rest/conf/cert.pem
