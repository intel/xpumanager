import stub
from flask import jsonify


def get_devices():
    code, message, data = stub.getDeviceList()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500


def get_device_properties(deviceId):
    try:
        code, message, data = stub.getDeviceProperties(int(deviceId))
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    except Exception as e:
        print(e)
        return "Internal error", 500
