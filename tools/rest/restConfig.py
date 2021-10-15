import configparser
import os
import sys
import hashlib

if __name__ == '__main__':
    print( "Please enter password for REST API user xpumadmin" )
    while True:
        pw = input( 'enter password:' )
        pwAgain = input( 'reenter password:' )
        if pw == pwAgain:
            break
        else:
            print( 'The two passwords you enter are inconsistent' )

    salt = os.popen( 'tr -dc A-Za-z0-9 < /dev/urandom |head -c 12' ).read()
    pwHash = hashlib.sha512( ( salt + pw ).encode( 'ASCII' ) ).hexdigest()

    config = configparser.ConfigParser()
    config['default'] = {}
    config['default']['username'] = 'xpumadmin'
    config['default']['salt'] = salt
    config['default']['password'] = pwHash
    
    path = os.path.dirname( os.path.realpath( __file__ ) )
    with open( path + '/rest/rest.conf', 'w' ) as configFile:
        config.write( configFile )

    print( 'rest.conf has been generated succefully' )
    print( 'If you forget the password, just re-run this script again' )
