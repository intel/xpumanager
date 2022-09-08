#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file statistics.py
#

from google.protobuf import empty_pb2
from .grpc_stub import stub, exit_on_disconnect
import core_pb2
import datetime
from .xpum_enums import XpumStatsType, XpumEngineType


@exit_on_disconnect
def getStatistics(device_id, session_id=0, get_accumulated=False):
    resp = stub.getStatistics(core_pb2.XpumGetStatsRequest(
        deviceId=device_id, sessionId=session_id))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["device_id"] = device_id

    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.begin/1e3, datetime.timezone.utc)
    endTimestamp = datetime.datetime.fromtimestamp(
        resp.end/1e3, datetime.timezone.utc)
    data['begin'] = beginTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    data['end'] = endTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')

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


@exit_on_disconnect
def getStatisticsByGroup(group_id, session_id=0, get_accumulated=False):
    resp = stub.getStatisticsByGroup(core_pb2.XpumGetStatsByGroupRequest(
        groupId=group_id, sessionId=session_id))
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
    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.begin/1e3, datetime.timezone.utc)
    endTimestamp = datetime.datetime.fromtimestamp(
        resp.end/1e3, datetime.timezone.utc)
    for deviceId in deviceMap:
        data = dict()
        data['device_id'] = deviceId
        data['begin'] = beginTimestamp.isoformat(
            timespec='milliseconds').replace('+00:00', 'Z')
        data['end'] = endTimestamp.isoformat(
            timespec='milliseconds').replace('+00:00', 'Z')
        data["device_level"] = deviceMap[deviceId]["device_level"]
        data["tile_level"] = deviceMap[deviceId]["tile_level"]
        datas.append(data)
    return 0, "OK", dict(group_id=group_id, datas=datas)


@exit_on_disconnect
def getEngineStatistics(device_id, session_id=0):
    resp = stub.getEngineStatistics(core_pb2.XpumGetEngineStatsRequest(
        deviceId=device_id, sessionId=session_id))
    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None
    data = dict()
    data["device_id"] = device_id
    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.begin/1e3, datetime.timezone.utc)
    endTimestamp = datetime.datetime.fromtimestamp(
        resp.end/1e3, datetime.timezone.utc)
    data['begin'] = beginTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    data['end'] = endTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    engine_stats = dict()
    for stats_info in resp.dataList:
        tmp = dict()
        try:
            engineType = XpumEngineType(stats_info.engineType).name
        except:
            engineType = stats_info.engineType
        tmp["engine_id"] = stats_info.engineId
        scale = stats_info.scale
        if scale == 1:
            tmp["value"] = stats_info.value
            tmp["avg"] = stats_info.avg
            tmp["min"] = stats_info.min
            tmp["max"] = stats_info.max
        else:
            tmp["value"] = stats_info.value / scale
            tmp["avg"] = stats_info.avg / scale
            tmp["min"] = stats_info.min / scale
            tmp["max"] = stats_info.max / scale
        tileId = stats_info.tileId if stats_info.isTileData else "device_level"
        if tileId not in engine_stats:
            engine_stats[tileId] = dict()
            engine_stats[tileId]["compute"] = []
            engine_stats[tileId]["render"] = []
            engine_stats[tileId]["decoder"] = []
            engine_stats[tileId]["encoder"] = []
            engine_stats[tileId]["copy"] = []
            engine_stats[tileId]["media_enhancement"] = []
            engine_stats[tileId]["3d"] = []
        if engineType == "XPUM_ENGINE_TYPE_COMPUTE":
            engine_stats[tileId]["compute"].append(tmp)
        elif engineType == "XPUM_ENGINE_TYPE_RENDER":
            engine_stats[tileId]["render"].append(tmp)
        elif engineType == "XPUM_ENGINE_TYPE_DECODE":
            engine_stats[tileId]["decoder"].append(tmp)
        elif engineType == "XPUM_ENGINE_TYPE_ENCODE":
            engine_stats[tileId]["encoder"].append(tmp)
        elif engineType == "XPUM_ENGINE_TYPE_COPY":
            engine_stats[tileId]["copy"].append(tmp)
        elif engineType == "XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT":
            engine_stats[tileId]["media_enhancement"].append(tmp)
        elif engineType == "XPUM_ENGINE_TYPE_3D":
            engine_stats[tileId]["3d"].append(tmp)

    data["engine_util"] = engine_stats
    return 0, "OK", data


@exit_on_disconnect
def getFabricStatistics(device_id, session_id=0, get_accumulated=False):
    resp = stub.getFabricStatistics(core_pb2.GetFabricStatsRequest(
        deviceId=device_id, sessionId=session_id
    ))
    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None
    data = dict()
    data["device_id"] = device_id
    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.begin/1e3, datetime.timezone.utc)
    endTimestamp = datetime.datetime.fromtimestamp(
        resp.end/1e3, datetime.timezone.utc)
    data['begin'] = beginTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    data['end'] = endTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    fabric_stats = []
    for stats_info in resp.dataList:
        tmp = dict()
        if stats_info.type == 1 or (stats_info.type == 3 and get_accumulated):
            tmp["name"] = "{}/{}->{}/{}".format(device_id, stats_info.tileId,
                                                stats_info.remote_device_id, stats_info.remote_device_tile_id)
            tmp["src_device_id"] = device_id
            tmp["src_tile_id"] = stats_info.tileId
            tmp["dst_device_id"] = stats_info.remote_device_id
            tmp["dst_tile_id"] = stats_info.remote_device_tile_id
        elif stats_info.type == 0 or (stats_info.type == 2 and get_accumulated):
            tmp["name"] = "{}/{}->{}/{}".format(stats_info.remote_device_id,
                                                stats_info.remote_device_tile_id, device_id, stats_info.tileId)
            tmp["src_device_id"] = stats_info.remote_device_id
            tmp["src_tile_id"] = stats_info.remote_device_tile_id
            tmp["dst_device_id"] = device_id
            tmp["dst_tile_id"] = stats_info.tileId
        else:
            continue

        tmp["type"] = stats_info.type
        scale = stats_info.scale
        if scale == 1:
            tmp["value"] = stats_info.value
            tmp["avg"] = stats_info.avg
            tmp["min"] = stats_info.min
            tmp["max"] = stats_info.max
            if get_accumulated:
                tmp["acc"] = stats_info.accumulated
        else:
            tmp["value"] = stats_info.value / scale
            tmp["avg"] = stats_info.avg / scale
            tmp["min"] = stats_info.min / scale
            tmp["max"] = stats_info.max / scale
            if get_accumulated:
                tmp["acc"] = stats_info.accumulated / scale
        fabric_stats.append(tmp)
    data["fabric_throughput"] = fabric_stats

    return 0, "OK", data


def getStatisticsNotForPrometheus(device_id, session_id=0, get_accumulated=False):
    resp = stub.getStatisticsNotForPrometheus(
        core_pb2.XpumGetStatsRequest(deviceId=device_id, sessionId=session_id, enableFilter=True))
    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None
    data = dict()
    data["device_id"] = device_id

    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.begin/1e3, datetime.timezone.utc)
    endTimestamp = datetime.datetime.fromtimestamp(
        resp.end/1e3, datetime.timezone.utc)
    data['begin'] = beginTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    data['end'] = endTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')

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
    resp = stub.getStatisticsByGroupNotForPrometheus(
        core_pb2.XpumGetStatsByGroupRequest(groupId=group_id, sessionId=session_id, enableFilter=True))
    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None

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
    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.begin/1e3, datetime.timezone.utc)
    endTimestamp = datetime.datetime.fromtimestamp(
        resp.end/1e3, datetime.timezone.utc)
    for deviceId in deviceMap:
        data = dict()
        data['device_id'] = deviceId
        data['begin'] = beginTimestamp.isoformat(
            timespec='milliseconds').replace('+00:00', 'Z')
        data['end'] = endTimestamp.isoformat(
            timespec='milliseconds').replace('+00:00', 'Z')
        data["device_level"] = deviceMap[deviceId]["device_level"]
        data["tile_level"] = deviceMap[deviceId]["tile_level"]
        datas.append(data)
    return 0, "OK", dict(group_id=group_id, datas=datas)
