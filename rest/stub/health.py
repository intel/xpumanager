from .grpc_stub import stub
import core_pb2

healthTypeEnumToString = {
    core_pb2.HEALTH_CORE_THERMAL: "core_temperature",
    core_pb2.HEALTH_MEMORY_THERMAL: "memory_temperature",
    core_pb2.HEALTH_POWER: "power",
    core_pb2.HEALTH_MEMORY: "memory",
    core_pb2.HEALTH_FABRIC_PORT: "xe_link_port"
}

healthStatusEnumToString = {
    core_pb2.HEALTH_STATUS_UNKNOWN: "Unknown",
    core_pb2.HEALTH_STATUS_OK: "OK",
    core_pb2.HEALTH_STATUS_WARNING: "Warning",
    core_pb2.HEALTH_STATUS_CRITICAL: "Critical"
}

def appendHealthThreshold(healthData, healthType):
    if healthType == 0:
        healthData['throttle_threshold'] = 105
        healthData['shutdown_threshold'] = 130
    elif healthType == 1:
        healthData['throttle_threshold'] = 85
        healthData['shutdown_threshold'] = 100
    elif healthType == 2:
        healthData['throttle_threshold'] = 150

def getHealth(deviceId, healthType):
    types = []
    healthTypes = ["coreTemperature", "memoryTemperature", "power", "memory", "xeLinkPort"]
    if healthType == "All":
        types = [0, 1, 2, 3, 4]
    else:
        types.append(healthTypes.index(healthType))
    data = dict()
    data['device_id'] = deviceId

    for t in types:
        resp = stub.getHealth(
            core_pb2.HealthDataRequest(deviceId=deviceId, type=t))
        if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None
        key = healthTypeEnumToString[t]
        data[key] = dict()
        data[key]['status'] = healthStatusEnumToString[resp.statusType]
        data[key]['description'] = resp.description
        if t == 0 or t == 1 or t == 2:
            appendHealthThreshold(data[key], t)
            resp = stub.getHealthConfig(
                core_pb2.HealthConfigRequest(deviceId=deviceId, configType=t))
            if len(resp.errorMsg) != 0:
                return 1, resp.errorMsg, None
            data[key]['custom_threshold'] = resp.threshold

    return 0, "OK", data


def getHealthByGroup(groupId, healthType):
    types = []
    healthTypes = ["coreTemperature", "memoryTemperature", "power", "memory", "xeLinkPort"]
    if healthType == "All":
        types = [0, 1, 2, 3, 4]
    else:
        types.append(healthTypes.index(healthType))

    datas = []
    deviceIds = []
    for t in types:
        resp = stub.getHealthByGroup(
            core_pb2.HealthDataByGroup(groupId=groupId, type=t))
        if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None

        for healthData in resp.healthData:
            if healthData.deviceId not in deviceIds:
                deviceIds.append(healthData.deviceId)
                datas.append(dict(device_id=healthData.deviceId))

            for data in datas:
                if data['device_id'] == healthData.deviceId:
                    key = healthTypeEnumToString[t]
                    data[key] = dict()
                    data[key]['status'] = healthStatusEnumToString[healthData.statusType]
                    data[key]['description'] = healthData.description
                    if t == 0 or t == 1 or t == 2:
                        appendHealthThreshold(data[key], t)
                        resp = stub.getHealthConfig(core_pb2.HealthConfigRequest(
                            deviceId=data['device_id'], configType=t))
                        if len(resp.errorMsg) != 0:
                            return 1, resp.errorMsg, None
                        data[key]['custom_threshold'] = resp.threshold

    return 0, "OK", dict(group_id=groupId, device_count=len(datas), device_list=datas)


def setHealthConfig(deviceId, healthType, threshold):
    if threshold < -1:
        return 1, "invalid threshold", None
    healthTypes = ["coreTemperature", "memoryTemperature", "power"]
    t = healthTypes.index(healthType)
    resp = stub.setHealthConfig(core_pb2.HealthConfigRequest(
        deviceId=deviceId, configType=t, threshold=threshold))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}


def setHealthConfigByGroup(groupId, healthType, threshold):
    if threshold < -1:
        return 1, "invalid threshold", None
    healthTypes = ["coreTemperature", "memoryTemperature", "power"]
    t = healthTypes.index(healthType)
    resp = stub.setHealthConfigByGroup(core_pb2.HealthConfigByGroupRequest(
        groupId=groupId, configType=t, threshold=threshold))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}
