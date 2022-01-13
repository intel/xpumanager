#!/usr/bin/python3
import argparse
import json
import http.client
import urllib.request
import sys

import base64

__version__ = '0.0.1'

host = "localhost"
port = 30000
disableTLS = True
deviceId = 0
tileId = None
username = "xpumadmin"
password = "user@123"


def genBasicAuthKey(username, password):
    credentials = ('%s:%s' % (username, password))
    encoded_credentials = base64.b64encode(credentials.encode('ascii'))
    return 'Basic %s' % encoded_credentials.decode("ascii")


def getStats():
    if disableTLS:
        conn = http.client.HTTPConnection(host, port)
    else:
        conn = http.client.HTTPSConnection(host, port)
    headers = {
        'Authorization': genBasicAuthKey(username, password)
    }
    url = "/rest/v1/devices/{}/stats".format(deviceId)
    conn.request("GET", url, "", headers)
    res = conn.getresponse()
    data = res.read()
    print(json.loads(data.decode("utf-8")))


def getHealth():
    pass


def arg():
    parser = argparse.ArgumentParser()
    parser.add_argument('-V', '--version', action='version',
                        version='%(prog)s v' + sys.modules[__name__].__version__)
    subparsers = parser.add_subparsers(
        title="Subcommand options", dest="subcommand")
    # telemetry
    telemetry_parser = subparsers.add_parser("telemetry", help="")
    # health
    health_parser = subparsers.add_parser("health", help="")

    parser.add_argument(
        '-H', '--host', help="The ip address or hostname of the host that runs XPU Manager Restful API service")
    parser.add_argument(
        '-p', '--port', help="The port of XPU Manager Restful API service")

    return parser.parse_args()


def main():
    parsed = arg()
    getStats()


if __name__ == "__main__":
    main()
