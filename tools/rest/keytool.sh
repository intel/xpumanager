#!/bin/bash

openssl req -x509 -sha512 -newkey rsa:3072 -nodes -keyout rest/key.pem -out rest/cert.pem -days 548 -subj "/ST=Unknown/O=XPUM"
