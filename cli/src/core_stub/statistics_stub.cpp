/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file statistics_stub.cpp
 */

#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

#include "core_stub.h"
#include "xpum_structs.h"
#include "xpum_api.h"
#include "lib_core_stub.h"
#include "exit_code.h"
#include "internal_api.h"

namespace xpum::cli {

inline bool metricsTypeAllowList(xpum_stats_type_t metricsType) {
    std::vector<xpum_stats_type_t> allowList{
        XPUM_STATS_GPU_UTILIZATION,
        XPUM_STATS_EU_ACTIVE,
        XPUM_STATS_EU_STALL,
        XPUM_STATS_EU_IDLE,
        XPUM_STATS_POWER,
        // XPUM_STATS_ENERGY,
        XPUM_STATS_GPU_FREQUENCY,
        XPUM_STATS_GPU_CORE_TEMPERATURE,
        XPUM_STATS_MEMORY_USED,
        // XPUM_STATS_MEMORY_UTILIZATION,
        XPUM_STATS_MEMORY_BANDWIDTH,
        // XPUM_STATS_MEMORY_READ,
        // XPUM_STATS_MEMORY_WRITE,
        XPUM_STATS_MEMORY_READ_THROUGHPUT,
        XPUM_STATS_MEMORY_WRITE_THROUGHPUT,
        XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION,
        XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION,
        // XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION,
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
        // XPUM_STATS_FREQUENCY_THROTTLE,
        // XPUM_STATS_PCIE_READ_THROUGHPUT,
        // XPUM_STATS_PCIE_WRITE_THROUGHPUT,
        // XPUM_STATS_PCIE_READ,
        // XPUM_STATS_PCIE_WRITE,
        XPUM_STATS_ENGINE_UTILIZATION,
    };
    return std::find(allowList.begin(), allowList.end(), metricsType) != allowList.end();
}

std::shared_ptr<std::map<int, std::map<int, int>>> LibCoreStub::getEngineCount(int deviceId) {
    std::map<int, std::map<int, int>> m;
    xpum_device_id_t xpum_device_id = deviceId;
    auto engineCountInfo = getDeviceAndTileEngineCount(xpum_device_id);
    for (auto tileEngineCountInfo : engineCountInfo) {
        std::map<int, int> mm;
        for (auto typeCountInfo : tileEngineCountInfo.engineCountList) {
            int engineType = typeCountInfo.engineType;
            int count = typeCountInfo.count;
            mm[engineType] = count;
        }
        int tileId = tileEngineCountInfo.isTileLevel ? tileEngineCountInfo.tileId : -1;
        m[tileId] = mm;
    }

    return std::make_shared<std::map<int, std::map<int, int>>>(m);
}

std::shared_ptr<nlohmann::json> LibCoreStub::getFabricCount(int deviceId) {
    nlohmann::json json;
    xpum_device_id_t xpum_device_id = deviceId;
    auto fabricCountInfo = getDeviceAndTileFabricCount(xpum_device_id);
    for (auto tileFabricCountInfo : fabricCountInfo) {
        std::string tileId = tileFabricCountInfo.isTileLevel ? std::to_string(tileFabricCountInfo.tileId) : "device";
        nlohmann::json obj;
        for (auto d : tileFabricCountInfo.dataList) {
            obj["tile_id"] = d.tile_id;
            obj["remote_device_id"] = d.remote_device_id;
            obj["remote_tile_id"] = d.remote_tile_id;
        }
        json[tileId].push_back(obj);
    }

    return std::make_shared<nlohmann::json>(json);
}

std::shared_ptr<nlohmann::json> LibCoreStub::getEngineStatistics(int deviceId) {
    nlohmann::json json;
    xpum_device_id_t xpum_device_id = deviceId;
    uint64_t sessionId = 0;
    uint32_t count = 128;
    uint64_t begin, end;
    xpum_device_engine_stats_t dataList[count];
    xpum_result_t res = xpumGetEngineStats(xpum_device_id, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        if (res == XPUM_METRIC_NOT_SUPPORTED || res == XPUM_METRIC_NOT_ENABLED)
            return std::make_shared<nlohmann::json>(json);
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                json["error"] = "Level Zero Initialization Error";
                json["errno"] = errorNumTranslate(res);
                break;
            default:
                json["error"] = "Error";
                json["errno"] = errorNumTranslate(res);
                break;
        }
        return std::make_shared<nlohmann::json>(json);
    }

    // engine data
    for (uint32_t i = 0; i < count; i++) {
        xpum_device_engine_stats_t& engineInfo = dataList[i];
        xpum_engine_type_t engineType = engineInfo.type;
        nlohmann::json obj;
        int32_t scale = engineInfo.scale;
        if (scale == 1) {
            obj["value"] = engineInfo.value;
        } else {
            obj["value"] = (double)engineInfo.value / scale;
        }
        obj["engine_id"] = engineInfo.index;
        std::string tileId = engineInfo.isTileData ? std::to_string(engineInfo.tileId) : "device";
        switch (engineType) {
            case XPUM_ENGINE_TYPE_COMPUTE:
                json[tileId]["compute"].push_back(obj);
                break;
            case XPUM_ENGINE_TYPE_RENDER:
                json[tileId]["render"].push_back(obj);
                break;
            case XPUM_ENGINE_TYPE_DECODE:
                json[tileId]["decoder"].push_back(obj);
                break;
            case XPUM_ENGINE_TYPE_ENCODE:
                json[tileId]["encoder"].push_back(obj);
                break;
            case XPUM_ENGINE_TYPE_COPY:
                json[tileId]["copy"].push_back(obj);
                break;
            case XPUM_ENGINE_TYPE_MEDIA_ENHANCEMENT:
                json[tileId]["media_enhancement"].push_back(obj);
                break;
            case XPUM_ENGINE_TYPE_3D:
                json[tileId]["3d"].push_back(obj);
                break;
            default:
                break;
        }
    }
    for (auto &item : json.items()) {
        auto key = item.key();
        auto &obj = json[key];
        if (!obj.contains("compute")) {
            obj["compute"] = nlohmann::json::array();
        }
        if (!obj.contains("render")) {
            obj["render"] = nlohmann::json::array();
        }
        if (!obj.contains("decoder")) {
            obj["decoder"] = nlohmann::json::array();
        }
        if (!obj.contains("encoder")) {
            obj["encoder"] = nlohmann::json::array();
        }
        if (!obj.contains("copy")) {
            obj["copy"] = nlohmann::json::array();
        }
        if (!obj.contains("media_enhancement")) {
            obj["media_enhancement"] = nlohmann::json::array();
        }
        if (!obj.contains("3d")) {
            obj["3d"] = nlohmann::json::array();
        }
    }

    return std::make_shared<nlohmann::json>(json);
}

std::shared_ptr<nlohmann::json> LibCoreStub::getFabricStatistics(int deviceId) {
    nlohmann::json json;
    xpum_device_id_t xpum_device_id = deviceId;
    uint64_t sessionId = 0;
    uint32_t count;
    uint64_t begin, end;
    auto res = xpumGetFabricThroughputStats(xpum_device_id, nullptr, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        if (res == XPUM_METRIC_NOT_SUPPORTED || res == XPUM_METRIC_NOT_ENABLED)
            return std::make_shared<nlohmann::json>(json);
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                json["error"] = "Level Zero Initialization Error";
                json["errno"] = errorNumTranslate(res);
                break;
            default:
                json["error"] = "Error";
                json["errno"] = errorNumTranslate(res);
                break;
        }
        return std::make_shared<nlohmann::json>(json);
    }
    xpum_device_fabric_throughput_stats_t dataList[count];
    res = xpumGetFabricThroughputStats(xpum_device_id, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK) {
        if (res == XPUM_METRIC_NOT_SUPPORTED || res == XPUM_METRIC_NOT_ENABLED)
            return std::make_shared<nlohmann::json>(json);
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                json["error"] = "Level Zero Initialization Error";
                json["errno"] = errorNumTranslate(res);
                break;
            default:
                json["error"] = "Error";
                json["errno"] = errorNumTranslate(res);
                break;
        }
        return std::make_shared<nlohmann::json>(json);
    }

    // fabric data
    json["fabric_throughput"] = nlohmann::json::array();
    for (uint32_t i = 0; i < count; i++) {
        xpum_device_fabric_throughput_stats_t& fabricInfo = dataList[i];
        nlohmann::json obj;
        std::stringstream ss;
        if (fabricInfo.type == XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED) {
            ss << deviceId << "/" << fabricInfo.tile_id << "->" << fabricInfo.remote_device_id << "/" << fabricInfo.remote_device_tile_id;
        } else if (fabricInfo.type == XPUM_FABRIC_THROUGHPUT_TYPE_RECEIVED) {
            ss << fabricInfo.remote_device_id << "/" << fabricInfo.remote_device_tile_id << "->" << deviceId << "/" << fabricInfo.tile_id;
        } else {
            continue;
        }

        int32_t scale = fabricInfo.scale;
        if (scale == 1) {
            obj["value"] = fabricInfo.value;
        } else {
            obj["value"] = (double)fabricInfo.value / scale;
        }
        obj["name"] = ss.str();
        obj["tile_id"] = fabricInfo.tile_id;
        json["fabric_throughput"].push_back(obj);
    }

    return std::make_shared<nlohmann::json>(json);
}

static int32_t getCliScale(xpum_stats_type_t metricsType) {
    switch (metricsType) {
        case XPUM_STATS_ENERGY:
            return 1000;
        case XPUM_STATS_MEMORY_USED:
            return 1048576;
        default:
            return 1;
    }
}

std::unique_ptr<nlohmann::json> LibCoreStub::getStatistics(int deviceId, bool enableFilter, bool enableScale) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_device_id_t xpum_device_id = deviceId;
    uint64_t sessionId = 0;
    uint32_t count = 5;
    xpum_device_stats_t dataList[count];
    uint64_t begin, end;
    xpum_result_t res = xpumGetStats(xpum_device_id, dataList, &count, &begin, &end, sessionId);
    if (res != XPUM_OK || count < 0) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "device not found";
                (*json)["errno"] = errorNumTranslate(res);
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                (*json)["errno"] = errorNumTranslate(res);
                break;
            default:
                (*json)["error"] = "Error";
                (*json)["errno"] = errorNumTranslate(res);
                break;
        }
        return json;
    }

    // get engine stats
    auto engineStatsJson = getEngineStatistics(deviceId);
    if (engineStatsJson->contains("error")) {
        return std::make_unique<nlohmann::json>(*engineStatsJson);
    }

    // get fabric stats
    auto fabricStatsJson = getFabricStatistics(deviceId);
    if (!fabricStatsJson->contains("error") && !fabricStatsJson->empty()) {
        json->update(*fabricStatsJson);
        // return std::make_unique<nlohmann::json>(*fabricStatsJson);
    }

    std::vector<nlohmann::json> deviceLevelStatsDataList;
    std::vector<nlohmann::json> tileLevelStatsDataList;

    for (uint32_t i = 0; i < count; i++) {
        xpum_device_stats_t& stats_info = dataList[i];
        std::vector<nlohmann::json> dataList;
        for (int j = 0; j < stats_info.count; j++) {
            xpum_device_stats_data_t& stats_data = stats_info.dataList[j];
            if (enableFilter && !metricsTypeAllowList(stats_data.metricsType))
                continue;
            auto tmp = nlohmann::json();
            xpum_stats_type_t metricsType = stats_data.metricsType;
            tmp["metrics_type"] = metricsTypeToString(metricsType);
            int32_t cliScale = getCliScale(metricsType);
            int32_t scale = enableScale ? stats_data.scale * cliScale : stats_data.scale;
            if (scale == 1) {
                tmp["value"] = stats_data.value;
                if (stats_data.isCounter) {
                    tmp["value"] = stats_data.accumulated;
                }
            } else {
                tmp["value"] = (double)stats_data.value / scale;
                if (stats_data.isCounter) {
                    tmp["value"] = (double)stats_data.accumulated / scale;
                }
            }
            dataList.push_back(tmp);
        }
        if (stats_info.isTileData) {
            auto tmp = nlohmann::json();
            tmp["tile_id"] = stats_info.tileId;
            tmp["data_list"] = dataList;
            auto strTileId = std::to_string(stats_info.tileId);
            if (engineStatsJson->contains(strTileId)) {
                tmp["engine_util"] = (*engineStatsJson)[strTileId];
            }
            tileLevelStatsDataList.push_back(tmp);
        } else {
            deviceLevelStatsDataList.insert(deviceLevelStatsDataList.end(), dataList.begin(), dataList.end());
        }
    }

    if (engineStatsJson->contains("device")) {
        (*json)["engine_util"] = (*engineStatsJson)["device"];
    }
    (*json)["device_level"] = deviceLevelStatsDataList;
    if (tileLevelStatsDataList.size() > 0)
        (*json)["tile_level"] = tileLevelStatsDataList;

    (*json)["device_id"] = deviceId;

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getStatisticsByGroup(uint32_t groupId, bool enableFilter, bool enableScale) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}
} // end namespace xpum::cli
