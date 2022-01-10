#include <nlohmann/json.hpp>
#include <vector>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "core_stub.h"
#include "xpum_structs.h"

namespace xpum::cli {

struct MetricsTypeEntry{
    xpum_stats_type_t key;
    std::string keyStr;
};

static MetricsTypeEntry metricsTypeArray[]{
    {XPUM_STATS_GPU_UTILIZATION, "XPUM_STATS_GPU_UTILIZATION"},
    {XPUM_STATS_EU_ACTIVE, "XPUM_STATS_EU_ACTIVE"},
    {XPUM_STATS_EU_STALL, "XPUM_STATS_EU_STALL"},
    {XPUM_STATS_EU_IDLE, "XPUM_STATS_EU_IDLE"},
    {XPUM_STATS_POWER, "XPUM_STATS_POWER"},
    {XPUM_STATS_ENERGY, "XPUM_STATS_ENERGY"},
    {XPUM_STATS_GPU_FREQUENCY, "XPUM_STATS_GPU_FREQUENCY"},
    {XPUM_STATS_GPU_CORE_TEMPERATURE, "XPUM_STATS_GPU_CORE_TEMPERATURE"},
    {XPUM_STATS_MEMORY_USED, "XPUM_STATS_MEMORY_USED"},
    {XPUM_STATS_MEMORY_UTILIZATION, "XPUM_STATS_MEMORY_UTILIZATION"},
    {XPUM_STATS_MEMORY_BANDWIDTH, "XPUM_STATS_MEMORY_BANDWIDTH"},
    {XPUM_STATS_MEMORY_READ, "XPUM_STATS_MEMORY_READ"},
    {XPUM_STATS_MEMORY_WRITE, "XPUM_STATS_MEMORY_WRITE"},
    {XPUM_STATS_MEMORY_READ_THROUGHPUT, "XPUM_STATS_MEMORY_READ_THROUGHPUT"},
    {XPUM_STATS_MEMORY_WRITE_THROUGHPUT, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT"},
    {XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION"},
    {XPUM_STATS_RAS_ERROR_CAT_RESET, "XPUM_STATS_RAS_ERROR_CAT_RESET"},
    {XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_GPU_REQUEST_FREQUENCY, "XPUM_STATS_GPU_REQUEST_FREQUENCY"},
    {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE"},
    {XPUM_STATS_FREQUENCY_THROTTLE, "XPUM_STATS_FREQUENCY_THROTTLE"},
    {XPUM_STATS_PCIE_READ_THROUGHPUT, "XPUM_STATS_PCIE_READ_THROUGHPUT"},
    {XPUM_STATS_PCIE_WRITE_THROUGHPUT, "XPUM_STATS_PCIE_WRITE_THROUGHPUT"}};

std::string CoreStub::metricsTypeToString(xpum_stats_type_t metricsType) {
    for (auto item : metricsTypeArray) {
        if (item.key == metricsType) {
            return item.keyStr;
        }
    }
    return std::to_string(metricsType);
}

std::unique_ptr<nlohmann::json> CoreStub::getStatistics(int deviceId, bool enableFilter) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumGetStatsResponse response;
    XpumGetStatsRequest request;
    request.set_deviceid(deviceId);
    request.set_sessionid(0);
    request.set_enablefilter(enableFilter);
    grpc::Status status = stub->getStatisticsNotForPrometheus(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        return json;
    }

    std::vector<nlohmann::json> deviceJsonList;
    uint64_t begin = response.begin();
    uint64_t end = response.end();
    (*json)["begin"] = isotimestamp(begin);
    (*json)["end"] = isotimestamp(end);

    (*json)["elapsed_time"] = (end - begin) / 1000;

    std::vector<nlohmann::json> deviceLevelStatsDataList;
    std::vector<nlohmann::json> tileLevelStatsDataList;

    for (int i{0}; i < response.datalist_size(); i++) {
        auto &stats_info = response.datalist(i);
        std::vector<nlohmann::json> dataList;
        for (int j = 0; j < stats_info.datalist_size(); j++) {
            auto &stats_data = stats_info.datalist(j);
            auto tmp = nlohmann::json();
            xpum_stats_type_t metricsType = (xpum_stats_type_t)stats_data.metricstype().value();
            tmp["metrics_type"] = metricsTypeToString(metricsType);
            int32_t scale = stats_data.scale();
            if (scale == 1) {
                tmp["value"] = stats_data.value();
                if (!stats_data.iscounter()) {
                    tmp["avg"] = stats_data.avg();
                    tmp["min"] = stats_data.min();
                    tmp["max"] = stats_data.max();
                } else {
                    tmp["total"] = stats_data.accumulated();
                }
            } else {
                tmp["value"] = (double)stats_data.value() / scale;
                if (!stats_data.iscounter()) {
                    tmp["avg"] = (double)stats_data.avg() / scale;
                    tmp["min"] = (double)stats_data.min() / scale;
                    tmp["max"] = (double)stats_data.max() / scale;
                } else {
                    tmp["total"] = (double)stats_data.accumulated() / scale;
                }
            }
            dataList.push_back(tmp);
        }
        if (stats_info.istiledata()) {
            auto tmp = nlohmann::json();
            tmp["tile_id"] = stats_info.tileid();
            tmp["data_list"] = dataList;
            tileLevelStatsDataList.push_back(tmp);
        } else {
            deviceLevelStatsDataList.insert(deviceLevelStatsDataList.end(), dataList.begin(), dataList.end());
        }
    }
    (*json)["device_level"] = deviceLevelStatsDataList;
    if (tileLevelStatsDataList.size() > 0)
        (*json)["tile_level"] = tileLevelStatsDataList;

    (*json)["device_id"] = deviceId;
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getStatisticsByGroup(uint32_t groupId, bool enableFilter) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumGetStatsResponse response;
    XpumGetStatsByGroupRequest request;
    request.set_groupid(groupId);
    request.set_sessionid(0);
    request.set_enablefilter(enableFilter);
    grpc::Status status = stub->getStatisticsByGroupNotForPrometheus(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        return json;
    }

    auto deviceMap = nlohmann::json();

    for (int i{0}; i < response.datalist_size(); i++) {
        auto &stats_info = response.datalist(i);

        std::string key = std::to_string(stats_info.deviceid());

        if (!deviceMap.contains(key)) {
            auto tmp = nlohmann::json();
            tmp["device_level"] = std::vector<nlohmann::json>();
            tmp["tile_level"] = std::vector<nlohmann::json>();
            tmp["device_id"] = stats_info.deviceid();
            deviceMap[key] = tmp;
        }

        std::vector<nlohmann::json> dataList;

        for (int j = 0; j < stats_info.datalist_size(); j++) {
            auto &stats_data = stats_info.datalist(j);
            auto tmp = nlohmann::json();
            xpum_stats_type_t metricsType = (xpum_stats_type_t)stats_data.metricstype().value();
            tmp["metrics_type"] = metricsTypeToString(metricsType);
            int32_t scale = stats_data.scale();
            if (scale == 1) {
                tmp["value"] = stats_data.value();
                if (!stats_data.iscounter()) {
                    tmp["avg"] = stats_data.avg();
                    tmp["min"] = stats_data.min();
                    tmp["max"] = stats_data.max();
                } else {
                    tmp["total"] = stats_data.accumulated();
                }
            } else {
                tmp["value"] = (double)stats_data.value() / scale;
                if (!stats_data.iscounter()) {
                    tmp["avg"] = (double)stats_data.avg() / scale;
                    tmp["min"] = (double)stats_data.min() / scale;
                    tmp["max"] = (double)stats_data.max() / scale;
                } else {
                    tmp["total"] = (double)stats_data.accumulated() / scale;
                }
            }
            dataList.push_back(tmp);
        }

        if (stats_info.istiledata()) {
            auto tmp = nlohmann::json();
            tmp["tile_id"] = stats_info.tileid();
            tmp["data_list"] = dataList;
            deviceMap[key]["tile_level"].push_back(tmp);
        } else {
            deviceMap[key]["device_level"] = dataList;
        }
    }

    uint64_t begin = response.begin();
    uint64_t end = response.end();
    uint32_t elapsed_time = (end - begin) / 1000;
    std::string beginTimestamp = isotimestamp(begin);
    std::string endTimestamp = isotimestamp(end);

    auto datas = std::vector<nlohmann::json>();
    for (auto item : deviceMap.items()) {
        nlohmann::json data;
        data["begin"] = beginTimestamp;
        data["end"] = endTimestamp;
        data["elapsed_time"] = elapsed_time;
        data["device_id"] = item.value()["device_id"];
        data["device_level"] = item.value()["device_level"];
        data["tile_level"] = item.value()["tile_level"];
        datas.push_back(data);
    }

    (*json)["datas"] = datas;
    (*json)["group_id"] = groupId;

    return json;
}
} // end namespace xpum::cli
