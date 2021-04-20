import http.client
import argparse

import json

import pprint

conn = http.client.HTTPConnection("localhost", 5000)

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--device',action='store_true', help='list all device')

def arg():
    parsed = parser.parse_args()
    # print(parsed)
    if parsed.device:
        get_devices()
    

def get_devices():
    payload = ''
    headers = {}
    conn.request("GET", "/dgm/v1/devices", payload, headers)
    res = conn.getresponse()
    jsonData = res.read()

    # print(data.decode("utf-8"))
    data = json.loads(jsonData.decode("utf-8"))
    pretty(data)

    for d in data:
        deviceId = d['device_id']
        url = "/dgm/v1/devices/{}/measurements".format(deviceId)
        conn.request("GET", url, {"type":1}, headers)
        d_res = conn.getresponse()
        print(d_res.read())


def pretty(d, indent=0):
    if isinstance(d, list):
        for v in d:
            pretty(v, indent)
    elif isinstance(d, dict):
        for key, value in d.items():
            print('\t' * indent + str(key)+":", end="")
            if isinstance(value, dict) or isinstance(value, list):
                print()
                pretty(value, indent+1)
            else:
                print("\t"+value)
    else:
        print('\t' * indent + str(d))

if __name__ == "__main__":
    arg()