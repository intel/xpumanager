#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

umask 007
openssl req -x509 -sha512 -newkey rsa:3072 -nodes -keyout rest/conf/key.pem -out rest/conf/cert.pem -days 548 -subj "/ST=Unknown/O=XPUM"
chown xpum:xpum rest/conf/key.pem
chown xpum:xpum rest/conf/cert.pem
