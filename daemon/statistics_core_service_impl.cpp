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
        // XPUM_STATS_MEMORY_UTILIZATION,
        XPUM_STATS_MEMORY_BANDWIDTH,
        // XPUM_STATS_MEMORY_READ,
        // XPUM_STATS_MEMORY_WRITE,
        XPUM_STATS_MEMORY_READ_THROUGHPUT,
        XPUM_STATS_MEMORY_WRITE_THROUGHPUT,
        // XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION,
        // XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION,
        // XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION,
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
    int count = 5;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStats(deviceId, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK || count < 0) {
        response->set_errormsg("Error");
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (int i = 0; i < count; i++) {
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
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getStatisticsByGroup(::grpc::ServerContext* context, const ::XpumGetStatsByGroupRequest* request, ::XpumGetStatsResponse* response) {
    xpum_device_id_t groupId = request->groupid();
    uint64_t sessionId = request->sessionid();
    int count = 5 * XPUM_MAX_NUM_DEVICES;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStatsByGroup(groupId, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("Group not found");
                return grpc::Status::OK;
            default:
                response->set_errormsg("Generic error");
                return grpc::Status::OK;
        }
    }
    response->set_begin(begin);
    response->set_end(end);
    for (int i = 0; i < count; i++) {
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
    int count = 5;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStats(deviceId, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK || count < 0) {
        response->set_status(res);
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND) {
            response->set_errormsg("device not found");
        } else {
            response->set_errormsg("fail to get statistics data");
        }
        return grpc::Status::OK;
    }
    response->set_begin(begin);
    response->set_end(end);
    for (int i = 0; i < count; i++) {
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
    int count = 5 * XPUM_MAX_NUM_DEVICES;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStatsByGroup(groupId, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
                response->set_errormsg("Group not found");
                return grpc::Status::OK;
            default:
                response->set_errormsg("Generic error");
                return grpc::Status::OK;
        }
    }
    response->set_begin(begin);
    response->set_end(end);
    for (int i = 0; i < count; i++) {
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
    if (res != XPUM_OK) {
        response->set_errormsg("Fail to get engine statistics data count");
        response->set_status(res);
        return grpc::Status::OK;
    }
    xpum_device_engine_stats_t dataList[count];
    res = xpumGetEngineStats(deviceId, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        response->set_errormsg("Fail to get engine statistics");
        response->set_status(res);
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
        engineStatsInfo->set_engineid(stats.id);
        engineStatsInfo->set_enginetype(stats.type);
        engineStatsInfo->set_value(stats.type);
        engineStatsInfo->set_min(stats.min);
        engineStatsInfo->set_avg(stats.avg);
        engineStatsInfo->set_max(stats.max);
        engineStatsInfo->set_scale(stats.scale);
    }
    return grpc::Status::OK;
}
}