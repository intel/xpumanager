from google.protobuf import empty_pb2
from .grpc_stub import stub
import core_pb2
import datetime
from enum import Enum

XpumStatsType = Enum("xpum_stats_type_t", (
    "XPUM_STATS_GPU_UTILIZATION",
    "XPUM_STATS_EU_ACTIVE",
    "XPUM_STATS_EU_STALL",
    "XPUM_STATS_EU_IDLE",
    "XPUM_STATS_POWER",
    "XPUM_STATS_ENERGY",
    "XPUM_STATS_GPU_FREQUENCY",
    "XPUM_STATS_GPU_TEMPERATURE",
    "XPUM_STATS_MEMORY_USED",
    "XPUM_STATS_MEMORY_UTILIZATION",
    "XPUM_STATS_MEMORY_BANDWIDTH",
    "XPUM_STATS_MEMORY_READ",
    "XPUM_STATS_MEMORY_WRITE",
    "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION",
    "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION",
    "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION",
    "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION",
    "XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION",
    "XPUM_STATS_RAS_ERROR_CAT_RESET",
    "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS",
    "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS",
    "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE",
    "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE",
    "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE",
    "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE",
    "XPUM_STATS_GPU_REQUEST_FREQUENCY",
    "XPUM_STATS_MAX"
), start=0)


def getStatistics(device_id, session_id=0, get_accumulated=False):
    resp = stub.getStatistics(core_pb2.XpumGetStatsRequest(deviceId=device_id, sessionId=session_id))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["device_id"] = device_id

    beginTimestamp = datetime.datetime.fromtimestamp(resp.begin/1e3)
    endTimestamp = datetime.datetime.fromtimestamp(resp.end/1e3)
    data['begin'] = beginTimestamp.isoformat(timespec='milliseconds')+"Z"
    data['end'] = endTimestamp.isoformat(timespec='milliseconds')+"Z"

    deviceLevelStatsDataList = []
    tileLevelStatsDataList = []
    for stats_info in resp.dataList:
        dataList = []
        for stats_data in stats_info.dataList:
            tmp = dict()
            try:
                metricsType = XpumStatsType(stats_data.metricsType.value).name
            except:
                metricsType = stats_data.metricsType.value
            tmp["metrics_type"] = metricsType
            tmp["value"] = stats_data.value
            if not stats_data.isCounter:
                tmp["avg"] = stats_data.avg
                tmp["min"] = stats_data.min
                tmp["max"] = stats_data.max
            elif get_accumulated:
                tmp["acc"] = stats_data.accumulated
            dataList.append(tmp)
        if stats_info.isTileData:
            tmp = dict(tile_id=stats_info.tileId, data_list=dataList)
            tileLevelStatsDataList.append(tmp)
        else:
            deviceLevelStatsDataList = dataList
    data["device_level"] = deviceLevelStatsDataList
    if tileLevelStatsDataList:
        data["tile_level"] = tileLevelStatsDataList
    return 0, "OK", data


def getStatisticsByGroup(group_id, session_id=0, get_accumulated=False):
    resp = stub.getStatisticsByGroup(core_pb2.XpumGetStatsByGroupRequest(groupId=group_id, sessionId=session_id))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None

    deviceMap = dict()

    for deviceStats in resp.dataList:
        deviceId = deviceStats.deviceId
        if deviceId not in deviceMap:
            deviceMap[deviceId] = {
                "device_level": [],
                "tile_level": []
            }
        dataList = []

        for d in deviceStats.dataList:
            tmp = dict()
            metricsType = XpumStatsType(d.metricsType.value).name
            tmp["metrics_type"] = metricsType
            tmp["value"] = d.value
            if not d.isCounter:
                tmp["min"] = d.min
                tmp["avg"] = d.avg
                tmp["max"] = d.max
            elif get_accumulated:
                tmp["acc"] = d.accumulated
            dataList.append(tmp)
        if deviceStats.isTileData:
            deviceMap[deviceId]["tile_level"].append({
                "data_list": dataList,
                "tile_id": deviceStats.tileId
            })
        else:
            deviceMap[deviceId]["device_level"] = dataList

    datas = []
    beginTimestamp = datetime.datetime.fromtimestamp(resp.begin/1e3)
    endTimestamp = datetime.datetime.fromtimestamp(resp.end/1e3)
    for deviceId in deviceMap:
        data = dict()
        data['device_id'] = deviceId
        data['begin'] = beginTimestamp.isoformat(timespec='milliseconds')+"Z"
        data['end'] = endTimestamp.isoformat(timespec='milliseconds')+"Z"
        data["device_level"] = deviceMap[deviceId]["device_level"]
        data["tile_level"] = deviceMap[deviceId]["tile_level"]
        datas.append(data)
    return 0, "OK", dict(group_id=group_id, datas=datas)
