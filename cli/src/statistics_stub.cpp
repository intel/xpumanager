#include <nlohmann/json.hpp>
#include <vector>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "core_stub.h"
#include "xpum_structs.h"

namespace xpum::cli {

static std::string metricsTypeToString(xpum_stats_type_t metricsType) {
    switch (metricsType) {
        case XPUM_STATS_GPU_UTILIZATION:
            return "XPUM_STATS_GPU_UTILIZATION";
        case XPUM_STATS_OCCUPATION:
            return "XPUM_STATS_OCCUPATION";
        case XPUM_STATS_ISSUE_EFFICIENCY:
            return "XPUM_STATS_ISSUE_EFFICIENCY";
        case XPUM_STATS_EXECUTION_EFFICIENCY:
            return "XPUM_STATS_EXECUTION_EFFICIENCY";
        case XPUM_STATS_NON_OCCUPATION:
            return "XPUM_STATS_NON_OCCUPATION";
        case XPUM_STATS_POWER:
            return "XPUM_STATS_POWER";
        case XPUM_STATS_ENERGY:
            return "XPUM_STATS_ENERGY";
        case XPUM_STATS_GPU_FREQUENCY:
            return "XPUM_STATS_GPU_FREQUENCY";
        case XPUM_STATS_GPU_TEMPERATURE:
            return "XPUM_STATS_GPU_TEMPERATURE";
        case XPUM_STATS_MEMORY_USED:
            return "XPUM_STATS_MEMORY_USED";
        case XPUM_STATS_MEMORY_UTILIZATION:
            return "XPUM_STATS_MEMORY_UTILIZATION";
        case XPUM_STATS_MEMORY_BANDWIDTH:
            return "XPUM_STATS_MEMORY_BANDWIDTH";
        case XPUM_STATS_MEMORY_READ:
            return "XPUM_STATS_MEMORY_READ";
        case XPUM_STATS_MEMORY_WRITE:
            return "XPUM_STATS_MEMORY_WRITE";
        case XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
            return "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION";
        case XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
            return "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION";
        case XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION:
            return "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION";
        case XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
            return "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION";
        case XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION:
            return "XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION";
        case XPUM_STATS_RAS_ERROR_CAT_RESET:
            return "XPUM_STATS_RAS_ERROR_CAT_RESET";
        case XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            return "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS";
        case XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS:
            return "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS";
        case XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            return "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE";
        case XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            return "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE";
        case XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
            return "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE";
        case XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
            return "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE";
        case XPUM_STATS_GPU_REQUEST_FREQUENCY:
            return "XPUM_STATS_GPU_REQUEST_FREQUENCY";
        default:
            break;
    }
    return std::to_string(metricsType);
}

std::unique_ptr<nlohmann::json> CoreStub::getStatistics(int deviceId) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumGetStatsResponse response;
    DeviceId grpcDeviceId;
    grpcDeviceId.set_id(deviceId);
    grpc::Status status = stub->getStatistics(&context, grpcDeviceId, &response);

    if (!status.ok()) {
        (*json)["Error"] = status.error_message();
    }

    if (response.errormsg().length() != 0) {
        (*json)["Error"] = response.errormsg();
        return json;
    }

    std::vector<nlohmann::json> deviceJsonList;
    uint64_t begin = response.begin();
    uint64_t end = response.end();
    (*json)["begin"] = isotimestamp(begin);
    (*json)["end"] = isotimestamp(end);

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
            tmp["value"] = stats_data.value();
            if (!stats_data.iscounter()) {
                tmp["avg"] = stats_data.avg();
                tmp["min"] = stats_data.min();
                tmp["max"] = stats_data.max();
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

    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getStatisticsByGroup(int groupId) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumGetStatsResponse response;
    GroupId grpcGroupId;
    grpcGroupId.set_id(groupId);
    grpc::Status status = stub->getStatisticsByGroup(&context, grpcGroupId, &response);

    if (!status.ok()) {
        (*json)["Error"] = status.error_message();
    }

    if (response.errormsg().length() != 0) {
        (*json)["Error"] = response.errormsg();
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
            tmp["value"] = stats_data.value();
            if (stats_data.iscounter()) {
                tmp["avg"] = stats_data.avg();
                tmp["min"] = stats_data.min();
                tmp["max"] = stats_data.max();
            }
            dataList.push_back(tmp);
        }

        if (stats_info.istiledata()) {
            auto tmp = nlohmann::json();
            tmp["tile_id"] = stats_info.tileid();
            tmp["data_list"] = dataList;
            deviceMap[stats_info.deviceid()]["tile_level"].push_back(tmp);
        } else {
            deviceMap[stats_info.deviceid()]["device_level"] = dataList;
        }
    }

    uint64_t begin = response.begin();
    uint64_t end = response.end();
    std::string beginTimestamp = isotimestamp(begin);
    std::string endTimestamp = isotimestamp(end);

    auto datas = std::vector<nlohmann::json>();
    for (auto item : deviceMap.items()) {
        nlohmann::json data;
        data["begin"] = beginTimestamp;
        data["end"] = endTimestamp;
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
