#!/bin/bash

umask 007
openssl req -x509 -sha512 -newkey rsa:3072 -nodes -keyout rest/key.pem -out rest/cert.pem -days 548 -subj "/ST=Unknown/O=XPUM"
chown xpum:xpum rest/key.pem
chown xpum:xpum rest/cert.pem
