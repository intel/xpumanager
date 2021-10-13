#!/bin/bash

openssl req -x509 -sha512 -newkey rsa:3072 -nodes -keyout py/key.pem -out py/cert.pem -days 548 -subj "/ST=Unknown/O=XPUM"
