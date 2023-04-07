#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file config.py
#

from .grpc_stub import stub
import core_pb2
import xpum_logger as logger

StandbyModeEnumToString = {
    core_pb2.STANDBY_DEFAULT: "DEFAULT",
    core_pb2.STANDBY_NEVER: "NEVER"
}

SchedulerModeEnumToString = {
    core_pb2.SCHEDULER_TIMEOUT: "TIMEOUT",
    core_pb2.SCHEDULER_TIMESLICE: "TIMESLICE",
    core_pb2.SCHEDULER_EXCLUSIVE: "EXCLUSIVE",
    core_pb2.SCHEDULER_DEBUG: "COMPUTE_UNIT_DEBUG"
}


def getConfig(deviceId, tileId):
    if(tileId == -1):
        isTiledId = False
        tileId = 0
    else:
        isTiledId = True

    resp = stub.getDeviceConfig(core_pb2.ConfigDeviceDataRequest(
        deviceId=deviceId, isTileData=isTiledId, tileId=tileId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data['device_id'] = resp.deviceId
    data['power_limit'] = resp.powerLimit
    data['power_vaild_range'] = resp.powerScope
    #data['power_average_window'] = resp.interval
    #data['power_average_window_vaild_range'] = resp.intervalScope
    #data['tileCount'] = resp.tileCount

    tilelist = list()
    for i in range(0, resp.tileCount):
        tiledata = dict()
        tiledata['tile_id'] = resp.tileConfigData[i].tileId
        #tiledata['power_limit'] = resp.tileConfigData[i].powerLimit
        #tiledata['power_vaild_range'] = resp.tileConfigData[i].powerScope
        #tiledata['power_average_window'] = resp.tileConfigData[i].interval
        #tiledata['power_average_window_vaild_range'] = resp.tileConfigData[i].intervalScope
        tiledata['gpu_frequency_valid_options'] = resp.tileConfigData[i].freqOption
        tiledata['min_frequency'] = resp.tileConfigData[i].minFreq
        tiledata['max_frequency'] = resp.tileConfigData[i].maxFreq
        tiledata['standby_mode'] = StandbyModeEnumToString[resp.tileConfigData[i].standby]
        tiledata['standby_mode_valid_options'] = resp.tileConfigData[i].standbyOption
        tiledata['scheduler_mode'] = SchedulerModeEnumToString[resp.tileConfigData[i].scheduler]
        if int(resp.tileConfigData[i].computePerformanceFactor) != -1:
            tiledata['compute_performance_factor'] = str(
                resp.tileConfigData[i].computePerformanceFactor)
        else:
            tiledata['compute_performance_factor'] = ''
        if int(resp.tileConfigData[i].mediaPerformanceFactor) != -1:
            tiledata['media_performance_factor'] = str(
                resp.tileConfigData[i].mediaPerformanceFactor)
        else:
            tiledata['media_performance_factor'] = ''
        tiledata['fabric_port_enabled'] = resp.tileConfigData[i].portEnabled
        tiledata['fabric_port_disabled'] = resp.tileConfigData[i].portDisabled
        tiledata['fabric_port_beaconing_on'] = resp.tileConfigData[i].portBeaconingOn
        tiledata['fabric_port_beaconing_off'] = resp.tileConfigData[i].portBeaconingOff
        tiledata["compute_engine"] = "compute"
        tiledata["media_engine"] = "media"

        if resp.tileConfigData[i].schedulerTimeout > 0:
            tiledata['scheduler_watchdog_timeout'] = resp.tileConfigData[i].schedulerTimeout
        if resp.tileConfigData[i].schedulerTimesliceInterval > 0:
            tiledata['scheduler_timeslice_interval'] = resp.tileConfigData[i].schedulerTimesliceInterval
            tiledata['scheduler_timeslice_yield_timeout'] = resp.tileConfigData[i].schedulerTimesliceYieldTimeout
        #if resp.tileConfigData[i].memoryEccAvailable == True:
        #    tiledata['memory_ecc_available'] = "True"
        #else:
        #    tiledata['memory_ecc_available'] = "False"
        
        #if resp.tileConfigData[i].memoryEccConfigurable == True:
        #    tiledata['memory_ecc_configurable'] = "True"
        #else:
        #    tiledata['memory_ecc_configurable'] = "False"
        #tiledata['memory_ecc_current_state'] = resp.tileConfigData[i].memoryEccState
        #tiledata['memory_ecc_pending_state'] = resp.tileConfigData[i].memoryEccPendingState
        #tiledata['memory_ecc_pending_action'] = resp.tileConfigData[i].memoryEccPendingAction
        data['memory_ecc_current_state'] = resp.tileConfigData[i].memoryEccState
        data['memory_ecc_pending_state'] = resp.tileConfigData[i].memoryEccPendingState
        tilelist.append(tiledata)
    data['tile_config_data'] = tilelist
    return 0, "OK", data


def setStandby(deviceId, tileId, standby):
    if standby.lower() == "never":
        mode = core_pb2.STANDBY_NEVER
    elif standby.lower() == "default":
        mode = core_pb2.STANDBY_DEFAULT
    else:
        return 1, "Invalid Parameter: standby mode", None

    resp = stub.setDeviceStandbyMode(core_pb2.ConfigDeviceStandbyRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, standby=mode))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed",
                     "Failed to set standby mode {} to device {}", mode, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed",
                 "Succeed to set standby mode {} to device {}", mode, deviceId)
    return 0, "OK", {"result": "OK"}


def setPortEnabled(deviceId, tileId, port, enabled):
    resp = stub.setDeviceFabricPortEnabled(core_pb2.ConfigDeviceFabricPortEnabledRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, portNumber=port, enabled=enabled))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed",
                     "Failed to set fabric port enable mode {} to device {}", enabled, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed",
                 "Succeed to set fabric port enable mode {} to device {}", enabled, deviceId)
    return 0, "OK", {"result": "OK"}


def setPerformanceFactor(deviceId, tileId, engineValue, factor):
    resp = stub.setPerformanceFactor(core_pb2.PerformanceFactor(
        deviceId=deviceId, isTileData=True, tileId=tileId, engineSet=engineValue, factor=factor))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed", "Failed to set engine {} performance factor {} to device {}",
                     engineValue, factor, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed", "Succeed to set engine {} performance factor {} to device {}",
                 engineValue, factor, deviceId)
    return 0, "OK", {"result": "OK"}


def setPortBeaconing(deviceId, tileId, port, beaconing):
    resp = stub.setDeviceFabricPortBeaconing(core_pb2.ConfigDeviceFabricPortBeconingRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, portNumber=port, beaconing=beaconing))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed",
                     "Failed to set port {} beaconing {} to device {}", port, beaconing, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed",
                 "Succeed to set port {} beaconing {} to device {}", port, beaconing, deviceId)
    return 0, "OK", {"result": "OK"}


def setPowerLimit(deviceId, tileId, power, interval):
    resp = stub.setDevicePowerLimit(core_pb2.ConfigDevicePowerLimitRequest(
        deviceId=deviceId, tileId=tileId, powerLimit=power, intervalWindow=interval))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed",
                     "Failed to set power {} interval {} to device {}", power, interval, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed",
                 "Succeed to set power {} interval {} to device {}", power, interval, deviceId)
    return 0, "OK", {"result": "OK"}


def setFrequencyRange(deviceId, tileId, minFreq, maxFreq):
    if maxFreq < minFreq:
        return 1, "maxFreq < minFreq", None

    resp = stub.setDeviceFrequencyRange(core_pb2.ConfigDeviceFrequencyRangeRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, minFreq=minFreq, maxFreq=maxFreq))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed",
                     "Failed to set min_frequency {} max_frequency {} to device {}", minFreq, maxFreq, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed",
                 "Succeed to set min_frequency {} max_frequency {} to device {}", minFreq, maxFreq, deviceId)
    return 0, "OK", {"result": "OK"}


def setScheduler(deviceId, tileId, mode, val1, val2):
    if mode.lower() == "timeout":
        scheduler = core_pb2.SCHEDULER_TIMEOUT
    elif mode.lower() == "timeslice":
        scheduler = core_pb2.SCHEDULER_TIMESLICE
    elif mode.lower() == "exclusive":
        scheduler = core_pb2.SCHEDULER_EXCLUSIVE
    elif mode.lower() == "debug":
        scheduler = core_pb2.SCHEDULER_DEBUG
    else:
        return 1, "Invalid Parameter: scheduler mode", None
    resp = stub.setDeviceSchedulerMode(core_pb2.ConfigDeviceSchdeulerModeRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, scheduler=scheduler, val1=val1, val2=val2))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed",
                     "Failed to set scheduler mode {} parameter1 {} parameter2 {} to device {}", mode, val1, val2, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed",
                 "Succeed to set scheduler mode {} parameter1 {} parameter2 {} to device {}", mode, val1, val2, deviceId)
    return 0, "OK", {"result": "OK"}


def setMemoryecc(deviceId, enabled):
    resp = stub.setDeviceMemoryEccState(core_pb2.ConfigDeviceMemoryEccStateRequest(
        deviceId=deviceId, enabled=enabled))
    if len(resp.errorMsg) != 0:
        logger.audit("Config", "Failed", "Failed to set memory ecc available {} configurable {} current {} pending {} action {}",
                     resp.available, resp.configurable, resp.currentState, resp.pendingState, resp.pendingAction)
        return 1, resp.errorMsg, None
    logger.audit("Config", "Succeed", "Succeed to set memory ecc available {} configurable {} current {} pending {} action {}",
                 resp.available, resp.configurable, resp.currentState, resp.pendingState, resp.pendingAction)
    return 0, "OK", {"result": "OK"}


def runReset(deviceId, force):
    resp = stub.ResetDevice(core_pb2.ResetDeviceRequest(
        deviceId=deviceId, force=force))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}
