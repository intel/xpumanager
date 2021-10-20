from google.protobuf import empty_pb2
from .grpc_stub import stub
import core_pb2
import datetime

from .exporter import XpumStatsType

def getStatistics(device_id):
    resp = stub.getStatistics(core_pb2.DeviceId(id=device_id))
    if len( resp.errorMsg ) != 0:
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
        dataList=[]
        for stats_data in stats_info.dataList:
            try:
                tmp = dict()
                metricsType = XpumStatsType(stats_data.metricsType.value).name
                tmp["metrics_type"] = metricsType
                tmp["value"] = stats_data.value
                if not stats_data.isCounter:
                    tmp["avg"] = stats_data.avg
                    tmp["min"] = stats_data.min
                    tmp["max"] = stats_data.max
                dataList.append(tmp)
            except:
                pass
        if stats_info.isTileData:
            tmp = dict(tile_id=stats_info.tileId,data_list=dataList)
            tileLevelStatsDataList.append(tmp)
        else:
            deviceLevelStatsDataList=dataList
    data["device_level"] = deviceLevelStatsDataList
    if tileLevelStatsDataList:
        data["tile_level"] = tileLevelStatsDataList
    return 0, "OK", data


def getStatisticsByGroup(group_id):
    resp = stub.getStatistics(core_pb2.GroupId(id=group_id))
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

        for d in deviceStats.dataList[:deviceStats.count]:
            tmp = dict()
            metricsType = XpumStatsType(d.metricsType).name
            tmp["metrics_type"] = metricsType
            tmp["value"] = d.value
            if not d.isCounter:
                tmp["min"] = d.min
                tmp["avg"] = d.avg
                tmp["max"] = d.max
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
        data['begin'] = str(beginTimestamp)
        data['end'] = str(endTimestamp)
        data["device_level"] = deviceMap[deviceId]["device_level"]
        data["tile_level"] = deviceMap[deviceId]["tile_level"]
        datas.append(data)
    return 0, "OK", dict(group_id=group_id, datas=datas)
