#!/usr/bin/python3

from flask import Flask, jsonify
from flask import render_template
from flask import request

from DGMCore import DGMCore


app = Flask(__name__)

core = DGMCore()

@app.route('/dgm/v1/version', methods=['GET'])
def get_version():
    count = core.getXpumVersion()
    return jsonify(count)
    

@app.route('/dgm/v1/devices', methods=['GET'])
def get_devices():
    devices = core.getDeviceList()
    return jsonify(devices)


@app.route('/dgm/v1/devices/<string:deviceId>/measurements', methods=['GET'])
def get_measurement(deviceId):
    measurementType = request.args.get("type", 0)
    realtime = request.args.get("realtime", False)

    if isinstance(measurementType, int):
        measurementTypeInt = measurementType
    elif isinstance(measurementType, str):
        measurementTypeInt = int(measurementType)
    else:
        measurementTypeInt = 0

    measurementTypeStr = ["power","frequency","temp","memory","engine"]
        
    if realtime:
        data = core.getRealtimeMeasurementData(deviceId, measurementTypeInt)
    else:
        data = core.getLatestMeasurementData(deviceId, measurementTypeInt)

    measurement = {
        "id": deviceId,
       measurementTypeStr[measurementTypeInt]: data
    }
    return jsonify(measurement)

if __name__ == '__main__':
  app.debug = True
  app.run()
