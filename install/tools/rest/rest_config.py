import configparser
import os
import pwd
import grp
import sys
import hashlib
import getpass

if __name__ == '__main__':
    print( "Please enter password for REST API user xpumadmin" )
    while True:
        pw = getpass.getpass( 'enter password:' )
        pwAgain = getpass.getpass( 'reenter password:' )
        if pw == pwAgain:
            break
        else:
            print( 'The two passwords you enter are inconsistent' )

    salt = os.popen( 'tr -dc A-Za-z0-9 < /dev/urandom |head -c 12' ).read()
    pwHash = hashlib.pbkdf2_hmac('sha512', pw.encode('ASCII'), salt.encode('ASCII'), 10000).hex()

    config = configparser.ConfigParser()
    config['default'] = {}
    config['default']['username'] = 'xpumadmin'
    config['default']['salt'] = salt
    config['default']['password'] = pwHash
    
    path = os.path.dirname( os.path.realpath( __file__ ) )
    os.umask( 0o007 )
    with open( path + '/rest/conf/rest.conf', 'w' ) as configFile:
        config.write( configFile )
    os.chown( path + '/rest/conf/rest.conf', pwd.getpwnam( 'xpum' ).pw_uid, grp.getgrnam( 'xpum' ).gr_gid )

    print( 'rest.conf has been generated succefully' )
    print( 'If you forget the password, just re-run this script again' )
