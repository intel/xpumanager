#!/usr/bin/env python3
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file rest_config.py
#

import configparser
import os
import pwd
import grp
import hashlib
import getpass
import argparse

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument( "-o", "--owner", help="owner of the generated config file" )
    parser.add_argument( "-g", "--group", help="group of the generated config file" )
    args = parser.parse_args()
    owner = args.owner if args.owner else 'xpum'
    group = args.group if args.group else 'xpum'

    print( "Please enter password for REST API user xpumadmin, min 8 characters" )
    while True:
        pw = getpass.getpass( 'Enter password:' )
        if len( pw ) < 8:
            print( 'Error: Password length shoube be at least 8 characters' )
            continue
        pwAgain = getpass.getpass( 'Confirm password:' )
        if pw == pwAgain:
            break
        else:
            print( 'Error: The two passwords you entered are inconsistent' )

    salt = os.popen( 'tr -dc A-Za-z0-9 < /dev/urandom |head -c 12' ).read()
    pwHash = hashlib.pbkdf2_hmac('sha512', pw.encode('ASCII'), salt.encode('ASCII'), 10000).hex()

    config = configparser.ConfigParser()
    config['default'] = {}
    config['default']['username'] = 'xpumadmin'
    config['default']['salt'] = salt
    config['default']['password'] = pwHash
    
    path = os.getcwd()
    os.umask( 0o007 )
    with open( path + '/rest/conf/rest.conf', 'w' ) as configFile:
        config.write( configFile )
    os.chown( path + '/rest/conf/rest.conf', pwd.getpwnam( owner ).pw_uid, grp.getgrnam( group ).gr_gid )

    print( 'rest.conf has been generated succefully' )
    print( 'If you forget the password, just re-run this script again' )
