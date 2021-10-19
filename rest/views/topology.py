from flask import jsonify
import stub

def get_topology(deviceId):
    code, message, data = stub.getTopology(deviceId)
    if code == 0:
        return jsonify(data)
    else:
        return message, 500