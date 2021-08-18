#!/usr/bin/python3

from flask import Flask, jsonify
from flask import render_template
from flask import request

from DGMCore import DGMCore


app = Flask(__name__)

core = DGMCore()

@app.route('/rest/v1/version', methods=['GET'])
def get_version():
    res = core.getVersion()
    return jsonify(res)
    

@app.route('/rest/v1/devices', methods=['GET'])
def get_devices():
    devices = core.getDeviceList()
    return jsonify(devices)


@app.route('/rest/v1/devices/<string:deviceId>', methods=['GET'])
def get_device_properties(deviceId):
    res = core.getDeviceProperties(int(deviceId))
    return jsonify(res)

if __name__ == '__main__':
  app.debug = True
#   app.run()
  app.run(port=30000)
