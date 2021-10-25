from .grpc_stub import stub
from enum import Enum
import core_pb2

XpumStatsType = Enum("xpum_stats_type_t", (
    "XPUM_STATS_GPU_UTILIZATION",
    "XPUM_STATS_OCCUPATION",
    "XPUM_STATS_ISSUE_EFFICIENCY",
    "XPUM_STATS_EXECUTION_EFFICIENCY",
    "XPUM_STATS_NON_OCCUPATION",
    "XPUM_STATS_POWER",
    "XPUM_STATS_ENERGY",
    "XPUM_STATS_GPU_FREQUENCY",
    "XPUM_STATS_GPU_TEMEPERATURE",
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
    "XPUM_STATS_PCIRX",
    "XPUM_STATS_PCITX",
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


def getMetrics(deviceId):
    resp = stub.getMetrics(core_pb2.DeviceId(id=deviceId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["DeviceId"] = deviceId
    deviceLevelStatsDataList = []
    tileLevelStatsDataList = []
    for stats_info in resp.dataList:
        dataList = []
        for stats_data in stats_info.dataList:
            try:
                tmp = dict()
                metricsType = XpumStatsType(stats_data.metricsType.value).name
                tmp["metricsType"] = metricsType
                tmp["value"] = stats_data.value
                if not stats_data.isCounter:
                    tmp["avg"] = stats_data.value
                dataList.append(tmp)
            except:
                pass
        if stats_info.isTileData:
            tmp = dict(tileId=stats_info.tileId, dataList=dataList)
            tileLevelStatsDataList.append(tmp)
        else:
            deviceLevelStatsDataList = dataList
    data["DeviceLevel"] = deviceLevelStatsDataList
    if tileLevelStatsDataList:
        data["TileLevel"] = tileLevelStatsDataList
    return 0, "OK", data
