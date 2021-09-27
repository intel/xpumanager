#!/usr/bin/python3

from flask import Flask, json, jsonify
from flask import render_template
from flask import request, Response
from requests import NullHandler

from DGMCore import DGMCore
from kube_pod_resource import get_pod_resources
from prometheus_exporter import get_metrics

app = Flask(__name__)

core = DGMCore()

@app.route('/metrics', methods=['GET'])
def export_metrics():
    return get_metrics(core, get_pod_resources())

@app.route('/rest/v1/version', methods=['GET'])
def get_version():
    code, message, data = core.getVersion()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500


@app.route('/rest/v1/devices', methods=['GET'])
def get_devices():
    code, message, data = core.getDeviceList()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500


@app.route('/rest/v1/devices/<int:deviceId>', methods=['GET'])
def get_device_properties(deviceId):
    try:
        code, message, data = core.getDeviceProperties(int(deviceId))
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    except Exception as e:
        print(e)
        return "Internal error", 500


@app.route('/rest/v1/groups', methods=['POST', 'GET'])
def groups():
    try:
        if request.method == 'POST':
            # create group
            req = request.get_json()
            groupName = req["GroupName"]
            code, message, data = core.createGroup(groupName)
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        elif request.method == 'GET':
            # get all group ids
            code, message, data = core.getAllGroupIds()
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
    except Exception as e:
        print(e)
        return "Internal error", 500


@app.route('/rest/v1/groups/<int:groupId>', methods=['GET', 'POST', 'DELETE'])
def group_detail(groupId):
    if request.method == 'GET':
        # get group info
        code, message, data = core.getGroupInfo(groupId)
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    elif request.method == 'DELETE':
        # destroy group
        code, message, data = core.destroyGroup(groupId)
        if code == 0:
            return "", 200
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    elif request.method == 'POST':
        req = request.get_json()

        # add device to group
        if "DeviceIdToAdd" in req:
            deviceIdToAdd = req["DeviceIdToAdd"]
            code, message, data = core.addDeviceToGroup(groupId, deviceIdToAdd)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400

        # remove device from group
        if "DeviceIdToRemove" in req:
            deviceIdToRemove = req["DeviceIdToRemove"]
            code, message, data = core.removeDeviceFromGroup(
                groupId, deviceIdToRemove)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
        return jsonify(data), 200


@app.route('/rest/v1/globalSettings', methods=['GET', 'POST'])
def agent_setting():
    if request.method == 'GET':
        code, message, data = core.getAllAgentConfig()
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        return jsonify(data), 200
    elif request.method == 'POST':
        req = request.get_json()
        if "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL" in req:
            value = req["XPUM_AGENT_CONFIG_SAMPLE_INTERVAL"]
            code, message, data = core.setAgentConfig(
                "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL", value)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
            return "", 200
        else:
            return dict(Status=1, Message="Illegal key"), 400


@app.route('/rest/v1/devices/<int:deviceId>/stats', methods=['GET'])
def get_statistics(deviceId):
    code, message, data = core.getStatistics(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/groups/<int:groupId>/stats', methods=['GET'])
def get_group_statistics(groupId):
    code, message, data = core.getStatisticsByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/devices/<int:deviceId>/firmwareUpgrade', methods=['POST'])
def run_firmware_flash(deviceId):
    firmwareType = request.form.get('type', type=int, default=0)
    filePath = request.form.get('file', type=str, default='')
    rc = core.runFirmwareFlash(deviceId, firmwareType, filePath)

    return jsonify({'result': rc})


@app.route('/rest/v1/devices/<int:deviceId>/firmware', methods=['GET'])
def get_firmware_flash_result(deviceId):
    firmwareType = request.args.get('type', type=int, default=0)

    rc = core.getFirmwareFlashResult(deviceId, firmwareType)

    return jsonify({'result': rc})


@app.route('/rest/v1/devices/<int:deviceId>/health', methods=['GET'])
def get_health_all(deviceId):
    code, message, data = core.getHealth(deviceId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/groups/<int:groupId>/health', methods=['GET'])
def get_group_health_all(groupId):
    code, message, data = core.getHealthByGroup(groupId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/devices/<int:deviceId>/health/<healthType>', methods=['GET'])
def get_health(deviceId, healthType):
    if healthType not in ["temperature", "power", "memory", "fabricPort"]:
        return

    code, message, data = core.getHealth(deviceId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/groups/<int:groupId>/health/<healthType>', methods=['GET'])
def get_group_health(groupId, healthType):
    if healthType not in ["temperature", "power", "memory", "fabricPort"]:
        return

    code, message, data = core.getHealthByGroup(groupId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/devices/<int:deviceId>/health/<healthType>', methods=['PUT'])
def set_health_config(deviceId, healthType):
    if healthType not in ["temperature", "power"]:
        return

    req = request.get_json()
    threshold = req["Threshold"]
    code, message, data = core.setHealthConfig(deviceId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/groups/<int:groupId>/health/<healthType>', methods=['PUT'])
def set_group_health_config(groupId, healthType):
    if healthType not in ["temperature", "power"]:
        return

    req = request.get_json()
    threshold = req["Threshold"]
    code, message, data = core.setHealthConfigByGroup(
        groupId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/devices/<int:deviceId>/diagnostics', methods=['POST'])
def run_diagnostics(deviceId):
    req = request.get_json()
    level = req["DiagnosticsLevel"]
    code, message, data = core.runDiagnostics(deviceId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/groups/<int:groupId>/diagnostics', methods=['POST'])
def run_group_diagnostics(groupId):
    req = request.get_json()
    level = req["DiagnosticsLevel"]
    code, message, data = core.runDiagnosticsByGroup(groupId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/devices/<int:deviceId>/diagnostics', methods=['GET'])
def get_diagnostics_result(deviceId):
    code, message, data = core.getDiagnosticsResult(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/groups/<int:groupId>/diagnostics', methods=['GET'])
def get_group_diagnostics_result(groupId):
    code, message, data = core.getDiagnosticsResultByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/devices/<int:deviceId>/standbys', methods=['GET','POST'])
def operate_device_standbys_result(deviceId):
    if request.method == 'GET':
        code, message, data = core.getDeviceStandbys(deviceId)
    else:
        if request.method == 'POST':
            req = request.get_json()
            subDeviceId = req["subDeviceId"]
            standbyType = req["standbyType"]
            mode = req["mode"]
            code, message, data = core.setDeviceStandby(deviceId, standbyType, subDeviceId, mode)
    
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/devices/<int:deviceId>/powerLimits', methods=['GET','POST'])
def operate_device_powerlimits_result(deviceId):
    req = request.get_json()
    subDeviceId = req["subDeviceId"]
    if subDeviceId is None:
        return jsonify("Need subDeviceId"), 500
    if request.method == 'GET':
        code, message, data = core.getDevicePowerLimits(deviceId, subDeviceId)
    else:
        if request.method == 'POST':
            limitType = req["limitType"]
            if limitType == 0: # Sustained limit
                enabled = req["enabled"]
                power = req["power"]
                interval = req["interval"]
                code, message, data = core.setDevicePowerSustainedLimits(deviceId, subDeviceId, enabled, power, interval)
            elif limitType == 1: # Burst limit
                enabled = req["enabled"]
                power = req["power"]
                code, message, data = core.setDevicePowerBurstLimits(deviceId, subDeviceId, enabled, power)
            elif limitType == 2: # Peak limit
                powerAC = req["powerAC"]
                powerDC = req["powerDC"]
                code, message, data = core.setDevicePowerPeakLimits(deviceId, subDeviceId, powerAC, powerDC)
            else:
                code, message, data = 500, "invalid limitType",
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/devices/<int:deviceId>/frequencyRanges', methods=['GET','POST'])
def operate_device_frequencyranges_result(deviceId):
    if request.method == 'GET':
        code, message, data = core.getDeviceFrequencyRanges(deviceId)
    else:
        if request.method == 'POST':
            req = request.get_json()
            subDeviceId = req["subDeviceId"]
            type = req["type"]
            min = req["min"]
            max = req["max"]
            code, message, data = core.setDeviceFrequencyRanges(deviceId, subDeviceId, type, min, max)
    
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/devices/<int:deviceId>/schedulers', methods=['GET','POST'])
def operate_device_schedulers_result(deviceId):
    if request.method == 'GET':
        code, message, data = core.getDeviceSchedulers(deviceId)
    else:
        if request.method == 'POST':
            req = request.get_json()
            subDeviceId = req["subDeviceId"]
            schedulerType = req["SchedulerType"]
            if schedulerType == 0: # Timeout mode
                watchdogTimeout = req["watchdogTimeout"]
                code, message, data = core.setDeviceSchedulerTimeoutMode(deviceId, subDeviceId, watchdogTimeout)
            elif schedulerType == 1: # Timeslice mode
                interval = req["interval"]
                yieldTimeout = req["yieldTimeout"]
                code, message, data = core.setDeviceSchedulerTimesliceMode(deviceId, subDeviceId, interval, yieldTimeout)
            elif schedulerType == 2: # Exclusive mode
                code, message, data = core.setDeviceSchedulerExclusiveMode(deviceId, subDeviceId)
            else:
                code, message, data = 500, "invalid scheduler Type",
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/devices/<int:deviceId>/reset', methods=['POST'])
def operate_device_reset(deviceId):
    if request.method == 'POST':
        code, message, data = core.resetDevice(deviceId)
    else:
        code, message, data = 500, "invalid reset request",
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/metricsRawDataTask', methods=['POST'])
def start_metrics_raw_data_collect_task():
    reqData = request.get_json()
    metricsTypeList = reqData['metricsTypeList']
    if "deviceId" in reqData:
        deviceId = reqData['deviceId']
        code, message, data = core.startCollectMetricsRawDataTask(
            deviceId, metricsTypeList)
    elif 'groupId' in reqData:
        groupId = reqData['groupId']
        code, message, data = core.startCollectMetricsRawDataTaskByGroup(
            groupId, metricsTypeList)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


@app.route('/rest/v1/metricsRawDataTask/<int:taskId>', methods=['POST', 'GET'])
def metrics_raw_data_collect_task(taskId):
    if request.method == 'POST':
        code, message, data = core.stopCollectMetricsRawDataTask(taskId)
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
        return "", 200
    elif request.method == 'GET':
        code, message, data = core.getMetricsRawDataByTask(taskId)
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
        return jsonify(data)

if __name__ == '__main__':
    app.debug = True
#   app.run()
    app.run(host='0.0.0.0', port=30000, use_reloader=False)
