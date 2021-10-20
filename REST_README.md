# XPUM REST API Setup Guide

##Installation
 1. install python3, may need to soft link python if your python package doesn't auto generate one.
 2. install pip3.
 3. install python packages as root for all user.  
    `umask 022`  
    `pip3 install flask flask_httpauth gunicorn[gevent]`
 
##Run Script
 1. Run restConfig.py to setup password for REST API user xpumadmin.  
 2. Run keytool.sh to generate self-signed certificate and keys.  
