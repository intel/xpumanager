/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file statistics_core_service_impl.cpp
 */

#include "internal_api.h"
#include "xpum_api.h"
#include "xpum_core_service_impl.h"
#include "xpum_structs.h"

namespace xpum::daemon {

inline bool metricsTypeAllowList(xpum_stats_type_t metricsType) {
    std::vector<xpum_stats_type_t> allowList{
        XPUM_STATS_GPU_UTILIZATION,
        XPUM_STATS_EU_ACTIVE,
        XPUM_STATS_EU_STALL,
        XPUM_STATS_EU_IDLE,
        XPUM_STATS_POWER,
        XPUM_STATS_ENERGY,
        XPUM_STATS_GPU_FREQUENCY,
        XPUM_STATS_GPU_CORE_TEMPERATURE,
        XPUM_STATS_MEMORY_USED,
        XPUM_STATS_MEMORY_UTILIZATION,
        XPUM_STATS_MEMORY_BANDWIDTH,
        // XPUM_STATS_MEMORY_READ,
        // XPUM_STATS_MEMORY_WRITE,
        XPUM_STATS_MEMORY_READ_THROUGHPUT,
        XPUM_STATS_MEMORY_WRITE_THROUGHPUT,
        XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION,
        XPUM_STATS_RAS_ERROR_CAT_RESET,
        XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
        XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS,
        XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
        XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
        // XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
        // XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
        XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE,
        XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE,
        // XPUM_STATS_GPU_REQUEST_FREQUENCY,
        XPUM_STATS_MEMORY_TEMPERATURE,
        XPUM_STATS_FREQUENCY_THROTTLE,
        XPUM_STATS_PCIE_READ_THROUGHPUT,
        XPUM_STATS_PCIE_WRITE_THROUGHPUT,
        XPUM_STATS_PCIE_READ,
        XPUM_STATS_PCIE_WRITE,
        XPUM_STATS_ENGINE_UTILIZATION,
    };
    return std::find(allowList.begin(), allowList.end(), metricsType) != allowList.end();
}

::grpc::Status XpumCoreServiceImpl::getStatistics(::grpc::ServerContext* context, const ::XpumGetStatsRequest* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t deviceId = request->deviceid();
    uint64_t sessionId = request->sessionid();
    uint32_t count = 5;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStats(deviceId, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Error");
                break;
        }
        response->set_errorno(res);
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_stats_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_stats_data_t& data = stats.dataList[j];
            // if (!metricsTypeAllowList(data.metricsType))
            //     continue;
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
            deviceStatsData->set_min(data.min);
            deviceStatsData->set_avg(data.avg);
            deviceStatsData->set_max(data.max);
            deviceStatsData->set_accumulated(data.accumulated);
            deviceStatsData->set_scale(data.scale);
        }
    }
    response->set_errorno(res);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getStatisticsByGroup(::grpc::ServerContext* context, const ::XpumGetStatsByGroupRequest* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t groupId = request->groupid();
    uint64_t sessionId = request->sessionid();
    uint32_t count = 5 * XPUM_MAX_NUM_DEVICES;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStatsByGroup(groupId, dataList, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("Group not found");
                return grpc::Status::OK;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                return grpc::Status::OK;
            default:
                response->set_errormsg("Generic error");
                return grpc::Status::OK;
        }
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_stats_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_stats_data_t& data = stats.dataList[j];
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
            deviceStatsData->set_min(data.min);
            deviceStatsData->set_avg(data.avg);
            deviceStatsData->set_max(data.max);
            deviceStatsData->set_accumulated(data.accumulated);
            deviceStatsData->set_scale(data.scale);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getStatisticsNotForPrometheus(::grpc::ServerContext* context, const ::XpumGetStatsRequest* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t deviceId = request->deviceid();
    uint64_t sessionId = request->sessionid();
    uint32_t count = 5;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStats(deviceId, dataList, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                response->set_errormsg("device not found");
                return grpc::Status::OK;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                return grpc::Status::OK;
            default:
                response->set_errormsg("fail to get statistics data");
                return grpc::Status::OK;
        }
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_stats_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_stats_data_t& data = stats.dataList[j];
            if (request->enablefilter() && !metricsTypeAllowList(data.metricsType))
                continue;
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
            deviceStatsData->set_min(data.min);
            deviceStatsData->set_avg(data.avg);
            deviceStatsData->set_max(data.max);
            deviceStatsData->set_accumulated(data.accumulated);
            deviceStatsData->set_scale(data.scale);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getStatisticsByGroupNotForPrometheus(::grpc::ServerContext* context, const ::XpumGetStatsByGroupRequest* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t groupId = request->groupid();
    uint64_t sessionId = request->sessionid();
    uint32_t count = 5 * XPUM_MAX_NUM_DEVICES;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStatsByGroup(groupId, dataList, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("Group not found");
                return grpc::Status::OK;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                return grpc::Status::OK;
            default:
                response->set_errormsg("Generic error");
                return grpc::Status::OK;
        }
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        DeviceStatsInfo* deviceStatsInfo = response->add_datalist();
        xpum_device_stats_t& stats = dataList[i];
        deviceStatsInfo->set_deviceid(stats.deviceId);
        deviceStatsInfo->set_istiledata(stats.isTileData);
        deviceStatsInfo->set_tileid(stats.tileId);
        deviceStatsInfo->set_count(stats.count);
        for (int j = 0; j < stats.count; j++) {
            xpum_device_stats_data_t& data = stats.dataList[j];
            if (request->enablefilter() && !metricsTypeAllowList(data.metricsType))
                continue;
            DeviceStatsData* deviceStatsData = deviceStatsInfo->add_datalist();
            deviceStatsData->mutable_metricstype()->set_value(data.metricsType);
            deviceStatsData->set_iscounter(data.isCounter);
            deviceStatsData->set_value(data.value);
            deviceStatsData->set_min(data.min);
            deviceStatsData->set_avg(data.avg);
            deviceStatsData->set_max(data.max);
            deviceStatsData->set_accumulated(data.accumulated);
            deviceStatsData->set_scale(data.scale);
        }
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getEngineStatistics(::grpc::ServerContext* context, const ::XpumGetEngineStatsRequest* request, ::XpumGetEngineStatsResponse* response) {
    xpum_device_id_t deviceId = request->deviceid();
    uint64_t sessionId = request->sessionid();
    uint32_t count;
    uint64_t begin, end;
    xpum_result_t res = xpumGetEngineStats(deviceId, nullptr, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Fail to get engine statistics data count");
                break;
        }
        return grpc::Status::OK;
    }
    xpum_device_engine_stats_t dataList[count];
    res = xpumGetEngineStats(deviceId, dataList, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            default:
                response->set_errormsg("Fail to get engine statistics");
                break;
        }
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        DeviceEngineStatsInfo* engineStatsInfo = response->add_datalist();
        xpum_device_engine_stats_t& stats = dataList[i];
        engineStatsInfo->set_deviceid(deviceId);
        engineStatsInfo->set_istiledata(stats.isTileData);
        engineStatsInfo->set_tileid(stats.tileId);
        engineStatsInfo->set_engineid(stats.index);
        engineStatsInfo->set_enginetype(stats.type);
        engineStatsInfo->set_value(stats.value);
        engineStatsInfo->set_min(stats.min);
        engineStatsInfo->set_avg(stats.avg);
        engineStatsInfo->set_max(stats.max);
        engineStatsInfo->set_scale(stats.scale);
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getEngineCount(::grpc::ServerContext* context, const ::GetEngineCountRequest* request, ::GetEngineCountResponse* response) {
    xpum_device_id_t deviceId = request->deviceid();
    auto engineCountInfo = getDeviceAndTileEngineCount(deviceId);
    for (auto tileEngineCountInfo : engineCountInfo) {
        auto countInfo = response->add_enginecountlist();
        countInfo->set_istilelevel(tileEngineCountInfo.isTileLevel);
        countInfo->set_tileid(tileEngineCountInfo.tileId);
        for (auto typeCountInfo : tileEngineCountInfo.engineCountList) {
            auto dataList = countInfo->add_datalist();
            dataList->set_enginetype(typeCountInfo.engineType);
            dataList->set_count(typeCountInfo.count);
        }
    }
    response->set_errorno(XPUM_OK);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFabricStatistics(::grpc::ServerContext* context, const ::GetFabricStatsRequest* request, ::GetFabricStatsResponse* response) {
    xpum_device_id_t deviceId = request->deviceid();
    uint64_t sessionId = request->sessionid();
    uint32_t count;
    uint64_t begin, end;
    auto res = xpumGetFabricThroughputStats(deviceId, nullptr, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            case XPUM_METRIC_NOT_SUPPORTED:
                response->set_errormsg("Metric not supported");
                break;
            case XPUM_METRIC_NOT_ENABLED:
                response->set_errormsg("Metric not enabled");
                break;
            default:
                response->set_errormsg("Fail to get fabric throughput statistics data count");
                break;
        }
        return grpc::Status::OK;
    }
    xpum_device_fabric_throughput_stats_t dataList[count];
    res = xpumGetFabricThroughputStats(deviceId, dataList, &count, &begin, &end, sessionId);
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            case XPUM_METRIC_NOT_SUPPORTED:
                response->set_errormsg("Metric not supported");
                break;
            case XPUM_METRIC_NOT_ENABLED:
                response->set_errormsg("Metric not enabled");
                break;
            default:
                response->set_errormsg("Fail to get fabric throughput statistics");
                break;
        }
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        FabricStatsInfo* fabricStatsInfo = response->add_datalist();
        xpum_device_fabric_throughput_stats_t& stats = dataList[i];
        fabricStatsInfo->set_tileid(stats.tile_id);
        fabricStatsInfo->set_remote_device_id(stats.remote_device_id);
        fabricStatsInfo->set_remote_device_tile_id(stats.remote_device_tile_id);
        fabricStatsInfo->set_type(stats.type);
        fabricStatsInfo->set_value(stats.value);
        fabricStatsInfo->set_min(stats.min);
        fabricStatsInfo->set_avg(stats.avg);
        fabricStatsInfo->set_max(stats.max);
        fabricStatsInfo->set_scale(stats.scale);
        fabricStatsInfo->set_accumulated(stats.accumulated);
        fabricStatsInfo->set_deviceid(stats.deviceId);
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFabricStatisticsEx(::grpc::ServerContext* context, const ::GetFabricStatsExRequest* request, ::GetFabricStatsResponse* response) {
    std::vector<xpum_device_id_t> deviceIdList;
    for (auto deviceId : request->deviceidlist()) {
        deviceIdList.push_back(deviceId);
    }
    uint64_t sessionId = request->sessionid();
    uint32_t count = deviceIdList.size() * 32;
    std::vector<xpum_device_fabric_throughput_stats_t> dataList(count);
    uint64_t begin, end;
    auto res = xpumGetFabricThroughputStatsEx(deviceIdList.data(), deviceIdList.size(), dataList.data(), &count, &begin, &end, sessionId);
    if (res == XPUM_BUFFER_TOO_SMALL) {
        dataList.reserve(count);
        res = xpumGetFabricThroughputStatsEx(deviceIdList.data(), deviceIdList.size(), dataList.data(), &count, &begin, &end, sessionId);
    }
    response->set_errorno(res);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                response->set_errormsg("Level Zero Initialization Error");
                break;
            case XPUM_METRIC_NOT_SUPPORTED:
                response->set_errormsg("Metric not supported");
                break;
            case XPUM_METRIC_NOT_ENABLED:
                response->set_errormsg("Metric not enabled");
                break;
            default:
                response->set_errormsg("Fail to get fabric throughput statistics");
                break;
        }
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (uint32_t i = 0; i < count; i++) {
        FabricStatsInfo* fabricStatsInfo = response->add_datalist();
        xpum_device_fabric_throughput_stats_t& stats = dataList[i];
        fabricStatsInfo->set_tileid(stats.tile_id);
        fabricStatsInfo->set_remote_device_id(stats.remote_device_id);
        fabricStatsInfo->set_remote_device_tile_id(stats.remote_device_tile_id);
        fabricStatsInfo->set_type(stats.type);
        fabricStatsInfo->set_value(stats.value);
        fabricStatsInfo->set_min(stats.min);
        fabricStatsInfo->set_avg(stats.avg);
        fabricStatsInfo->set_max(stats.max);
        fabricStatsInfo->set_scale(stats.scale);
        fabricStatsInfo->set_accumulated(stats.accumulated);
        fabricStatsInfo->set_deviceid(stats.deviceId);
    }
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFabricCount(::grpc::ServerContext* context, const ::GetFabricCountRequest* request, ::GetFabricCountResponse* response) {
    xpum_device_id_t deviceId = request->deviceid();
    auto fabricCountInfo = getDeviceAndTileFabricCount(deviceId);
    for (auto tileFabricCountInfo : fabricCountInfo) {
        auto countInfo = response->add_fabriccountlist();
        countInfo->set_istilelevel(tileFabricCountInfo.isTileLevel);
        countInfo->set_tileid(tileFabricCountInfo.tileId);
        for (auto d : tileFabricCountInfo.dataList) {
            auto dataList = countInfo->add_datalist();
            dataList->set_tileid(d.tile_id);
            dataList->set_remotedeviceid(d.remote_device_id);
            dataList->set_remotetileid(d.remote_tile_id);
        }
    }
    response->set_errorno(XPUM_OK);
    return grpc::Status::OK;
}
} // namespace xpum::daemon
