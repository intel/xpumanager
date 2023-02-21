# XPUM REST API Setup Guide

## Installation
 1. install python3, may need to soft link python if your python package doesn't auto generate one.
 2. install python3-devel ( Redhat or CentOS only )
 3. install pip3.
 4. install python packages as root for all user.  
    `umask 022`  
    `pip3 install -r INSTALLDIR/rest/requirements.txt`
 
## Run Script
 1. Run rest_config.py to setup password for REST API user xpumadmin.  
 2. Run keytool.sh to generate self-signed certificate and keys.  

## Start Service
 1. systemctl start xpum_rest.service
 2. systemctl enable xpum_rest.service

## Change rest service binding address & port
 1. change service file xpum_rest.service "--bind" parameters in "ExecStart"

## Replace rest service certificate & private key
 1. Replace certificate file cert.pem under INSTALLDIR/rest/conf
 2. Replace private key file key.pem under INSTALLDIR/rest/conf