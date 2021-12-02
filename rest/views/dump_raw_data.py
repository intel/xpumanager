from flask import request, jsonify
import stub


def startDumpRawDataTask():
    reqData = request.get_json()
    deviceId = reqData.get("device_id", None)
    if deviceId is None:
        error = dict(Message="Device id must be assigned", Status=1)
        return jsonify(error), 400
    metricsTypeList = reqData.get("metrics_type_list", [])
    if not metricsTypeList:
        error = dict(Message="`metrics_type_list` can not be empty", Status=1)
        return jsonify(error), 400
    tileId = reqData.get("tile_id", -1)
    code, message, data = stub.startDumpRawDataTask(
        deviceId, tileId, metricsTypeList)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def stopDumpRawDataTask(taskId):
    code, message, data = stub.stopDumpRawDataTask(taskId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def listDumpRawDataTasks():
    code, message, data = stub.listDumpRawDataTasks()
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
