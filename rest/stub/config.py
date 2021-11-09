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
    else:
        isTiledId = True
    
    resp = stub.getDeviceConfig(core_pb2.ConfigDeviceDataRequest(deviceId=deviceId, isTileData=isTiledId, tileId=tileId))
    if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None
    data = dict()
    data['deviceId'] = resp.deviceId
    data['powerLimit'] = resp.powerLimit
    data['interval'] = resp.interval
    data['tileCount'] = resp.tileCount

    for tile in resp.tileConfigData:
        tiledata = dict()
        tiledata['tileId'] = tile.tileId
        tiledata['minFreq'] = tile.minFreq
        tiledata['maxFreq'] = tile.maxFreq
        tiledata['standby'] = StandbyModeEnumToString[tile.standby]
        tiledata['scheduler'] = SchedulerModeEnumToString[tile.scheduler]
        data['tileConfigData'].append(tiledata)        
    return 0, "OK", data

def setsetStandby(deviceId, tileId, standby):
    resp = stub.setDeviceStandbyMode(core_pb2.ConfigDeviceStandbyRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, standby=standby))
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
    resp = stub.setDeviceFrequencyRange(core_pb2.ConfigDeviceFrequencyRangeRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, minFreq=minFreq, maxFreq=maxFreq))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}

def setScheduler(deviceId, tileId, mode, val1, val2):
    resp = stub.setDeviceSchedulerMode(core_pb2.ConfigDeviceSchdeulerModeRequest(
        deviceId=deviceId, isTileData=True, tileId=tileId, scheduler=mode, val1=val1, val2=val2))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}

def runReset(deviceId):
    #resp = stub.ResetDevice()
    #if len(resp.errorMsg) != 0:
    #    return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"} 
