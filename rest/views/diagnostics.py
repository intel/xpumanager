import stub
from flask import request
from flask import jsonify

def run_diagnostics(deviceId):
    req = request.get_json()
    level = req["DiagnosticsLevel"]
    code, message, data = stub.runDiagnostics(deviceId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def run_group_diagnostics(groupId):
    req = request.get_json()
    level = req["DiagnosticsLevel"]
    code, message, data = stub.runDiagnosticsByGroup(groupId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_diagnostics_result(deviceId):
    code, message, data = stub.getDiagnosticsResult(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_group_diagnostics_result(groupId):
    code, message, data = stub.getDiagnosticsResultByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)