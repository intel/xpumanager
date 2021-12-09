from google.protobuf import empty_pb2
from .grpc_stub import stub
import core_pb2
import datetime
from .xpum_enums import XpumStatsType

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
            scale = stats_data.scale
            if scale == 1:
                tmp["value"] = stats_data.value
                if not stats_data.isCounter:
                    tmp["avg"] = stats_data.avg
                    tmp["min"] = stats_data.min
                    tmp["max"] = stats_data.max
                elif get_accumulated:
                    tmp["acc"] = stats_data.accumulated
            else:
                tmp["value"] = stats_data.value / scale
                if not stats_data.isCounter:
                    tmp["avg"] = stats_data.avg / scale
                    tmp["min"] = stats_data.min / scale
                    tmp["max"] = stats_data.max / scale
                elif get_accumulated:
                    tmp["acc"] = stats_data.accumulated / scale
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
            scale = d.scale
            if scale == 1:
                tmp["value"] = d.value
                if not d.isCounter:
                    tmp["min"] = d.min
                    tmp["avg"] = d.avg
                    tmp["max"] = d.max
                elif get_accumulated:
                    tmp["acc"] = d.accumulated
            else:
                tmp["value"] = d.value / scale
                if not d.isCounter:
                    tmp["min"] = d.min / scale
                    tmp["avg"] = d.avg / scale
                    tmp["max"] = d.max / scale
                elif get_accumulated:
                    tmp["acc"] = d.accumulated / scale
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

def getStatisticsNotForPrometheus(device_id, session_id=0, get_accumulated=False):
    resp = stub.getStatisticsNotForPrometheus(core_pb2.XpumGetStatsRequest(deviceId=device_id, sessionId=session_id))
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
            scale = stats_data.scale
            if scale == 1:
                tmp["value"] = stats_data.value
                if not stats_data.isCounter:
                    tmp["avg"] = stats_data.avg
                    tmp["min"] = stats_data.min
                    tmp["max"] = stats_data.max
                elif get_accumulated:
                    tmp["acc"] = stats_data.accumulated
            else:
                tmp["value"] = stats_data.value / scale
                if not stats_data.isCounter:
                    tmp["avg"] = stats_data.avg / scale
                    tmp["min"] = stats_data.min / scale
                    tmp["max"] = stats_data.max / scale
                elif get_accumulated:
                    tmp["acc"] = stats_data.accumulated / scale
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


def getStatisticsByGroupNotForPrometheus(group_id, session_id=0, get_accumulated=False):
    resp = stub.getStatisticsByGroupNotForPrometheus(core_pb2.XpumGetStatsByGroupRequest(groupId=group_id, sessionId=session_id))
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
            scale = d.scale
            if scale == 1:
                tmp["value"] = d.value
                if not d.isCounter:
                    tmp["min"] = d.min
                    tmp["avg"] = d.avg
                    tmp["max"] = d.max
                elif get_accumulated:
                    tmp["acc"] = d.accumulated
            else:
                tmp["value"] = d.value / scale
                if not d.isCounter:
                    tmp["min"] = d.min / scale
                    tmp["avg"] = d.avg / scale
                    tmp["max"] = d.max / scale
                elif get_accumulated:
                    tmp["acc"] = d.accumulated / scale
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
