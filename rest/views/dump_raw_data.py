from flask import request, jsonify
import stub


def start_metrics_raw_data_collect_task():
    reqData = request.get_json()
    metricsTypeList = reqData['metricsTypeList']
    if "deviceId" in reqData:
        deviceId = reqData['deviceId']
        code, message, data = stub.startCollectMetricsRawDataTask(
            deviceId, metricsTypeList)
    elif 'groupId' in reqData:
        groupId = reqData['groupId']
        code, message, data = stub.startCollectMetricsRawDataTaskByGroup(
            groupId, metricsTypeList)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def metrics_raw_data_collect_task(taskId):
    if request.method == 'POST':
        code, message, data = stub.stopCollectMetricsRawDataTask(taskId)
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
        return "", 200
    elif request.method == 'GET':
        code, message, data = stub.getMetricsRawDataByTask(taskId)
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
        return data
