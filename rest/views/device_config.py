from flask import request, jsonify
import stub


def operate_device_standbys_result(deviceId):
    if request.method == 'GET':
        code, message, data = stub.getDeviceStandbys(deviceId)
    else:
        if request.method == 'POST':
            req = request.get_json()
            subDeviceId = req["subDeviceId"]
            standbyType = req["standbyType"]
            mode = req["mode"]
            code, message, data = stub.setDeviceStandby(
                deviceId, standbyType, subDeviceId, mode)

    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def operate_device_powerlimits_result(deviceId):
    req = request.get_json()
    subDeviceId = req["subDeviceId"]
    if subDeviceId is None:
        return jsonify("Need subDeviceId"), 500
    if request.method == 'GET':
        code, message, data = stub.getDevicePowerLimits(deviceId, subDeviceId)
    else:
        if request.method == 'POST':
            limitType = req["limitType"]
            if limitType == 0:  # Sustained limit
                enabled = req["enabled"]
                power = req["power"]
                interval = req["interval"]
                code, message, data = stub.setDevicePowerSustainedLimits(
                    deviceId, subDeviceId, enabled, power, interval)
            elif limitType == 1:  # Burst limit
                enabled = req["enabled"]
                power = req["power"]
                code, message, data = stub.setDevicePowerBurstLimits(
                    deviceId, subDeviceId, enabled, power)
            elif limitType == 2:  # Peak limit
                powerAC = req["powerAC"]
                powerDC = req["powerDC"]
                code, message, data = stub.setDevicePowerPeakLimits(
                    deviceId, subDeviceId, powerAC, powerDC)
            else:
                code, message, data = 500, "invalid limitType",
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def operate_device_frequencyranges_result(deviceId):
    if request.method == 'GET':
        code, message, data = stub.getDeviceFrequencyRanges(deviceId)
    else:
        if request.method == 'POST':
            req = request.get_json()
            subDeviceId = req["subDeviceId"]
            type = req["type"]
            min = req["min"]
            max = req["max"]
            code, message, data = stub.setDeviceFrequencyRanges(
                deviceId, subDeviceId, type, min, max)

    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def operate_device_schedulers_result(deviceId):
    if request.method == 'GET':
        code, message, data = stub.getDeviceSchedulers(deviceId)
    else:
        if request.method == 'POST':
            req = request.get_json()
            subDeviceId = req["subDeviceId"]
            schedulerType = req["SchedulerType"]
            if schedulerType == 0:  # Timeout mode
                watchdogTimeout = req["watchdogTimeout"]
                code, message, data = stub.setDeviceSchedulerTimeoutMode(
                    deviceId, subDeviceId, watchdogTimeout)
            elif schedulerType == 1:  # Timeslice mode
                interval = req["interval"]
                yieldTimeout = req["yieldTimeout"]
                code, message, data = stub.setDeviceSchedulerTimesliceMode(
                    deviceId, subDeviceId, interval, yieldTimeout)
            elif schedulerType == 2:  # Exclusive mode
                code, message, data = stub.setDeviceSchedulerExclusiveMode(
                    deviceId, subDeviceId)
            else:
                code, message, data = 500, "invalid scheduler Type",
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def operate_device_reset(deviceId):
    if request.method == 'POST':
        code, message, data = stub.resetDevice(deviceId)
    else:
        code, message, data = 500, "invalid reset request",
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
