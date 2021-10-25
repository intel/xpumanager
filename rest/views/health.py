import stub
from flask import request
from flask import jsonify


def get_health_all(deviceId):
    code, message, data = stub.getHealth(deviceId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_group_health_all(groupId):
    code, message, data = stub.getHealthByGroup(groupId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_health(deviceId, healthType):
    if healthType not in ["temperature", "power", "memory", "fabricPort"]:
        return

    code, message, data = stub.getHealth(deviceId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_group_health(groupId, healthType):
    if healthType not in ["temperature", "power", "memory", "fabricPort"]:
        return

    code, message, data = stub.getHealthByGroup(groupId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_health_config(deviceId, healthType):
    if healthType not in ["temperature", "power"]:
        return

    req = request.get_json()
    threshold = req["Threshold"]
    code, message, data = stub.setHealthConfig(deviceId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_group_health_config(groupId, healthType):
    if healthType not in ["temperature", "power"]:
        return

    req = request.get_json()
    threshold = req["Threshold"]
    code, message, data = stub.setHealthConfigByGroup(
        groupId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
