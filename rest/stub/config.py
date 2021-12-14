from .grpc_stub import stub
import core_pb2

StandbyModeEnumToString = {
    core_pb2.STANDBY_DEFAULT: "default",
    core_pb2.STANDBY_NEVER: "never"
}

SchedulerModeEnumToString = {
    core_pb2.SCHEDULER_TIMEOUT: "timeout",
    core_pb2.SCHEDULER_TIMESLICE: "timeslice",
    core_pb2.SCHEDULER_EXCLUSIVE: "exclusive"
}

def getConfig(deviceId, tileId):
    if(tileId == -1):
        isTiledId = False
        tileId = 0
    else:
        isTiledId = True
    
    resp = stub.getDeviceConfig(core_pb2.ConfigDeviceDataRequest(deviceId=deviceId, isTileData=isTiledId, tileId=tileId))
    if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None
    data = dict()
    data['device_id'] = resp.deviceId
    #data['power_limit'] = resp.powerLimit
    #data['power_average_window'] = resp.interval
    #data['tileCount'] = resp.tileCount

    tilelist = list()
    for i in range(0,resp.tileCount):
        tiledata = dict()
        tiledata['tile_id'] = resp.tileConfigData[i].tileId
        tiledata['power_limit'] = resp.tileConfigData[i].powerLimit
        tiledata['power_average_window'] = resp.tileConfigData[i].interval
        tiledata['min_frequency'] = resp.tileConfigData[i].minFreq
        tiledata['max_frequency'] = resp.tileConfigData[i].maxFreq
        tiledata['standby_mode'] = StandbyModeEnumToString[resp.tileConfigData[i].standby]
        tiledata['scheduler_mode'] = SchedulerModeEnumToString[resp.tileConfigData[i].scheduler]
        tilelist.append(tiledata)
    data['tile_config_data'] = tilelist
    return 0, "OK", data

def setStandby(deviceId, tileId, standby):
    if standby.lower() == "never":
        mode = core_pb2.STANDBY_NEVER
    elif standby.lower() == "default":
        mode = core_pb2.STANDBY_NEVER
    else:
        return 1, "Invalid Parameter", None
    
    resp = stub.setDeviceStandbyMode(core_pb2.ConfigDeviceStandbyRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, standby=mode))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}

def setPowerLimit(deviceId, power, interval):
    resp = stub.setDevicePowerLimit(core_pb2.ConfigDevicePowerLimitRequest(
        deviceId=deviceId, powerLimit=power, intervalWindow=interval))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}

def setFrequencyRange(deviceId, tileId, minFreq, maxFreq):
    if maxFreq < minFreq:
        return 1, "maxFreq < minFreq", None

    resp = stub.setDeviceFrequencyRange(core_pb2.ConfigDeviceFrequencyRangeRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, minFreq=minFreq, maxFreq=maxFreq))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}

def setScheduler(deviceId, tileId, mode, val1, val2):
    if mode.lower() == "timeout":
        scheduler = core_pb2.SCHEDULER_TIMEOUT
    elif mode.lower() == "timeslice":
        scheduler = core_pb2.SCHEDULER_TIMESLICE
    elif mode.lower() == "exclusive":
        scheduler = core_pb2.SCHEDULER_EXCLUSIVE
    else:
        return 1, "Invalid Parameter", None
    resp = stub.setDeviceSchedulerMode(core_pb2.ConfigDeviceSchdeulerModeRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, scheduler=scheduler, val1=val1, val2=val2))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}

def runReset(deviceId):
    #resp = stub.ResetDevice()
    #if len(resp.errorMsg) != 0:
    #    return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"} 
