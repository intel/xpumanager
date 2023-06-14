/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file grpc_core_stub.cpp
 */

#include "core_stub.h"


#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "logger.h"
#include "xpum_api.h"
#include "xpum_structs.h"
#include "internal_api.h"
#include "lib_core_stub.h"
#include "exit_code.h"

#include <iostream>

using namespace xpum;

namespace xpum::cli {

LibCoreStub::LibCoreStub(bool initCore) {
    char* env = std::getenv("SPDLOG_LEVEL");
    if (!env) {
        putenv(const_cast<char*>("SPDLOG_LEVEL=OFF"));
    }
    this->initCore = initCore;
    if (this->initCore)
        xpumInit();
}

LibCoreStub::~LibCoreStub() {
    if (this->initCore)
        xpumShutdown();
}

bool LibCoreStub::isChannelReady() {
    return true;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getVersion() {
     auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    const std::string notDetected = "Not Detected";

    (*json)["xpum_version"] = notDetected;
    (*json)["xpum_version_git"] = notDetected;
    (*json)["level_zero_version"] = notDetected;

    int count{0};
    xpum_result_t res = xpumVersionInfo(nullptr, &count);
    if (res == XPUM_OK) {
        xpum_version_info versions[count];
        res = xpumVersionInfo(versions, &count);
        if (res == XPUM_OK) {
            for (int i{0}; i < count; ++i) {
                switch (versions[i].version) {
                    case XPUM_VERSION:
                        (*json)["xpum_version"] = versions[i].versionString;
                        break;
                    case XPUM_VERSION_GIT:
                        (*json)["xpum_version_git"] = versions[i].versionString;
                        break;
                    case XPUM_VERSION_LEVEL_ZERO:
                        (*json)["level_zero_version"] = versions[i].versionString;
                        break;
                    default:
                        assert(0);
                }
            }
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeivceIdByBDF(const char* bdf, int *deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = xpumGetDeviceIdByBDF(bdf, deviceId);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "device not found";
                (*json)["errno"] = errorNumTranslate(res);
                break;
            default:
                (*json)["error"] = "Error";
                (*json)["errno"] = errorNumTranslate(res);
                break;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getTopology(int deviceId) {
    std::shared_ptr<xpum_topology_t> topo(static_cast<xpum_topology_t*>(malloc(sizeof(xpum_topology_t))), free);
    std::shared_ptr<xpum_topology_t> topology = topo;
    std::size_t size = sizeof(xpum_topology_t);
    xpum_result_t res = xpumGetTopology(deviceId, topology.get(), &size);
    if (res == XPUM_BUFFER_TOO_SMALL) {
        std::shared_ptr<xpum_topology_t> newTopo(static_cast<xpum_topology_t*>(malloc(size)), free);
        topology = newTopo;
        res = xpumGetTopology(deviceId, topology.get(), &size);
    }
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["device_id"] = deviceId;
    if (res == XPUM_OK && size > 0) {
        (*json)["affinity_localcpulist"] = topology->cpuAffinity.localCPUList;
        (*json)["affinity_localcpus"] = topology->cpuAffinity.localCPUs;
        (*json)["switch_count"] = topology->switchCount;
        std::vector<std::string> switchList;
        for (int i{0}; i < topology->switchCount; ++i) {
            switchList.push_back(topology->switches[i].switchDevicePath);
            (*json)["switch_list"] = switchList;
        }
    } else {
        (*json)["error"] = "Error";
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupCreate(std::string groupName) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupDelete(int groupId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupListAll() {
    using namespace xpum;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    int count = XPUM_MAX_NUM_GROUPS;
    xpum_result_t res = xpumGetAllGroupIds(nullptr, &count);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                return json;
            default:
                (*json)["error"] = "Error";
                return json;
        }
    }
    xpum_group_id_t groups[count];
    res = xpumGetAllGroupIds(groups, &count);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                return json;
            default:
                (*json)["error"] = "Error";
                return json;
        }
    }
    // response->set_count(count);

    std::vector<nlohmann::json> groupJsonList;

    for (int i{0}; i < count; i++) {
        xpum_group_info_t info;
        xpum_result_t res = xpumGroupGetInfo(groups[i], &info);
        if (res != XPUM_OK) {
            (*json)["error"] = "Error";
            return json;
        }
        auto groupJson = nlohmann::json();
        groupJson["group_id"] = groups[i];
        groupJson["group_name"] = info.groupName;
        groupJson["device_count"] = info.count;
        std::vector<int32_t> deviceIdList;
        for (int j{0}; j < info.count; j++) {
            deviceIdList.push_back(info.deviceList[j]);
        }
        groupJson["device_id_list"] = deviceIdList;

        groupJsonList.push_back(groupJson);
    }

    (*json)["group_list"] = groupJsonList;

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupList(int groupId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupAddDevice(int groupId, int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupRemoveDevice(int groupId, int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}


static std::string diagnosticResultEnumToString(xpum_diag_task_result_t result) {
    std::string ret;
    switch (result) {
        case xpum_diag_task_result_t::XPUM_DIAG_RESULT_UNKNOWN:
            ret = "Unknown";
            break;
        case xpum_diag_task_result_t::XPUM_DIAG_RESULT_PASS:
            ret = "Pass";
            break;
        case xpum_diag_task_result_t::XPUM_DIAG_RESULT_FAIL:
            ret = "Fail";
            break;
        default:
            break;
    }
    return ret;
}

static std::string diagnosticTypeEnumToString(xpum_diag_task_type_t type, bool rawComponentTypeStr) {
    std::string ret;
    switch (type) {
        case xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_ENV_VARIABLES:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_ENV_VARIABLES" : "Software Env Variables");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_LIBRARY:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_LIBRARY" : "Software Library");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_PERMISSION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_PERMISSION" : "Software Permission");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_SOFTWARE_EXCLUSIVE:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_EXCLUSIVE" : "Software Exclusive");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_HARDWARE_SYSMAN:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_HARDWARE_SYSMAN" : "Hardware Sysman");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_COMPUTATION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_COMPUTATION" : "Computation Check");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_LIGHT_CODEC:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_LIGHT_CODEC" : "Media Codec Check");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_INTEGRATION_PCIE:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_INTEGRATION_PCIE" : "Integration PCIe");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_MEDIA_CODEC:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_MEDIA_CODEC" : "Media Codec");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_COMPUTATION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_COMPUTATION" : "Performance Computation");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_POWER:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_POWER" : "Performance Power");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION" : "Performance Memory Allocation");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH" : "Performance Memory Bandwidth");
            break;
        case xpum_diag_task_type_t::XPUM_DIAG_MEMORY_ERROR:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_MEMORY_ERROR" : "Memory Error");
            break;
        default:
            break;
    }
    return ret;
}

static std::string diagnosticsMediaCodecResolutionEnumToString(xpum_media_resolution_t resolution) {
    std::string ret;
    switch (resolution) {
        case xpum_media_resolution_t::XPUM_MEDIA_RESOLUTION_1080P:
            ret = "1080p";
            break;
        case xpum_media_resolution_t::XPUM_MEDIA_RESOLUTION_4K:
            ret = "4K";
            break;
        default:
            break;
    }
    return ret;
}

static std::string diagnosticsMediaCodecFormatEnumToString(xpum_media_format_t format) {
    std::string ret;
    switch (format) {
        case xpum_media_format_t::XPUM_MEDIA_FORMAT_H265:
            ret = "H.265";
            break;
        case xpum_media_format_t::XPUM_MEDIA_FORMAT_H264:
            ret = "H.264";
            break;
        case xpum_media_format_t::XPUM_MEDIA_FORMAT_AV1:
            ret = "AV1";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> LibCoreStub::runDiagnostics(int deviceId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (level > 0)
        res = xpumRunDiagnostics(deviceId, static_cast<xpum_diag_level_t>(level));
    else if (targetTypes.size() > 0) {
        int count = targetTypes.size();
        xpum_diag_task_type_t types[count];
        for (int i = 0; i < count; i++)
            types[i] = static_cast<xpum_diag_task_type_t>(targetTypes[i]);
        res = xpumRunMultipleSpecificDiagnostics(deviceId, types, count);
    } else
        res = XPUM_GENERIC_ERROR;
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "device not found";
                (*json)["errno"] = errorNumTranslate(res);
                break;
            case XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE:
                (*json)["error"] = "last diagnostic task on the device is not completed";
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

    json = getDiagnosticsResult(deviceId, rawComponentTypeStr);
    if ((*json).contains("error")) {
        return json;
    }

    auto startTime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    while ((*json)["finished"] == false) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
        json = getDiagnosticsResult(deviceId, rawComponentTypeStr);
        if ((*json).contains("error")) {
            return json;
        }

        auto endTime = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
        if (endTime - startTime >= 30 * 60) {
            auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*errorJson)["error"] = "time out for unknown reasons";
            (*errorJson)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_TASK_TIMEOUT;
            return errorJson;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDiagnosticsResult(int deviceId, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_diag_task_info_t task_info;
    xpum_result_t res = xpumGetDiagnosticsResult(deviceId, &task_info);
    if (res == XPUM_OK) {
        (*json)["device_id"] = task_info.deviceId;
        if (task_info.level >= 1 && task_info.level <= 3)
            (*json)["level"] = task_info.level;
        (*json)["finished"] = task_info.finished;
        (*json)["message"] = task_info.message;
        (*json)["component_count"] = task_info.count;
        (*json)["start_time"] = isotimestamp(task_info.startTime);
        if (task_info.finished) {
            (*json)["end_time"] = isotimestamp(task_info.endTime);
        }
        (*json)["result"] = diagnosticResultEnumToString(task_info.result);
        if ((*json)["result"] != "Pass") {
            (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_TASK_FAILED;
        }
        std::vector<nlohmann::json> componentJsonList;
        for (int i = 0; i < task_info.count; ++i) {
            // disable XPUM_DIAG_HARDWARE_SYSMAN
            if (task_info.componentList[i].type == XPUM_DIAG_HARDWARE_SYSMAN) {
                (*json)["component_count"] = task_info.count - 1;
                continue;
            }
            auto componentJson = nlohmann::json();
            componentJson["component_type"] = diagnosticTypeEnumToString(task_info.componentList[i].type, rawComponentTypeStr);
            componentJson["finished"] = task_info.componentList[i].finished;
            componentJson["message"] = task_info.componentList[i].message;
            componentJson["result"] = diagnosticResultEnumToString(task_info.componentList[i].result);
            if (task_info.componentList[i].type == XPUM_DIAG_SOFTWARE_EXCLUSIVE) {
                uint32_t count = 0;
                res = xpumGetDeviceProcessState(task_info.deviceId, nullptr, &count);
                if (res == XPUM_OK) {
                    if (count > 1) {
                        xpum_device_process_t dataArray[count];
                        res = xpumGetDeviceProcessState(task_info.deviceId, dataArray, &count);
                        if (res == XPUM_OK) {
                            std::vector<nlohmann::json> processList;
                            for (uint i{0}; i < count; ++i) {
                                auto proc = nlohmann::json();
                                proc["process_id"] = dataArray[i].processId;
                                proc["process_name"] = dataArray[i].processName;
                                if (proc["process_name"] != "")
                                    processList.push_back(proc);
                            }
                            componentJson["process_list"] = processList;
                        }
                    }
                } else {
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
                }
            }
            if (task_info.componentList[i].type == XPUM_DIAG_MEDIA_CODEC 
                && task_info.componentList[i].result == XPUM_DIAG_RESULT_PASS) {
                componentJson["media_codec_list"] = (*getDiagnosticsMediaCodecResult(task_info.deviceId, rawComponentTypeStr))["media_codec_list"];
            }
            componentJsonList.push_back(componentJson);
        }
        (*json)["component_list"] = componentJsonList;
    } else {
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
    }
    return json;
}

std::shared_ptr<nlohmann::json> LibCoreStub::getDiagnosticsMediaCodecResult(int deviceId, bool rawFpsStr) {
    auto json = std::shared_ptr<nlohmann::json>(new nlohmann::json());
 int count = 6; // Resolution: 1080p, 4K; Format: H264, H265, AV1
    xpum_diag_media_codec_metrics_t resultList[6];
    xpum_result_t res = xpumGetDiagnosticsMediaCodecResult(deviceId, resultList, &count);
    if (res == XPUM_OK) {
        std::vector<nlohmann::json> mediaPerfJsonList;
        for (int i = 0; i < count; ++i) {
            auto perfJson = nlohmann::json();
            std::string resolution = diagnosticsMediaCodecResolutionEnumToString(resultList[i].resolution);
            std::string format = diagnosticsMediaCodecFormatEnumToString(resultList[i].format);
            if (rawFpsStr) {
                perfJson[resolution + " " + format] = resultList[i].fps;
            } else {
                perfJson["fps"] = " " + resolution + " " + format + " : " + resultList[i].fps;
            }

            mediaPerfJsonList.push_back(perfJson); 
        }
        (*json)["media_codec_list"] = mediaPerfJsonList;
    } else {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "device not found";
                (*json)["errno"] = errorNumTranslate(res);
                break;
            case XPUM_RESULT_DIAGNOSTIC_TASK_NOT_FOUND:
                (*json)["error"] = "task not found";
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
    }

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::runDiagnosticsByGroup(uint32_t groupId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

nlohmann::json LibCoreStub::appendHealthThreshold(int deviceId, nlohmann::json json, xpum_health_type_t type,
                                               uint64_t throttleValue, uint64_t shutdownValue) {
    if (type == xpum_health_type_t::XPUM_HEALTH_POWER) {
        json["throttle_threshold"] = throttleValue;
    }
    if (type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL) {
        json["throttle_threshold"] = throttleValue;
        json["shutdown_threshold"] = shutdownValue;
    }
    if (type == xpum_health_type_t::XPUM_HEALTH_MEMORY_THERMAL) {
        json["throttle_threshold"] = throttleValue;
        json["shutdown_threshold"] = shutdownValue;
    }
    return json;
}

static std::string healthStatusEnumToString(xpum_health_status_t status) {
    std::string ret;
    switch (status) {
        case xpum_health_status_t::XPUM_HEALTH_STATUS_UNKNOWN:
            ret = "Unknown";
            break;
        case xpum_health_status_t::XPUM_HEALTH_STATUS_OK:
            ret = "OK";
            break;
        case xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING:
            ret = "Warning";
            break;
        case xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL:
            ret = "Critical";
            break;
        default:
            break;
    }
    return ret;
}

static std::string healthTypeEnumToString(xpum_health_type_t type) {
    std::string ret;
    switch (type) {
        case xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL:
            ret = "core_temperature";
            break;
        case xpum_health_type_t::XPUM_HEALTH_MEMORY_THERMAL:
            ret = "memory_temperature";
            break;
        case xpum_health_type_t::XPUM_HEALTH_POWER:
            ret = "power";
            break;
        case xpum_health_type_t::XPUM_HEALTH_MEMORY:
            ret = "memory";
            break;
        case xpum_health_type_t::XPUM_HEALTH_FABRIC_PORT:
            ret = "xe_link_port";
            break;
        case xpum_health_type_t::XPUM_HEALTH_FREQUENCY:
            ret = "frequency";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllHealth() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    int count{XPUM_MAX_NUM_DEVICES};
    xpum_device_basic_info devices[XPUM_MAX_NUM_DEVICES];
    xpum_result_t res = xpumGetDeviceList(devices, &count);
    if (res == XPUM_OK) {
        std::vector<nlohmann::json> healthJsonList;
        for (int i = 0; i < count; i++) {
            auto healthJson = (*getHealth(devices[i].deviceId, -1));
            healthJsonList.push_back(healthJson);
        }
        (*json)["device_list"] = healthJsonList;
    } else {
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
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getHealth(int deviceId, int componentType) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    std::vector<xpum_health_type_t> types = {   XPUM_HEALTH_CORE_THERMAL,
                                                XPUM_HEALTH_MEMORY_THERMAL,
                                                XPUM_HEALTH_POWER,
                                                XPUM_HEALTH_MEMORY,
                                                XPUM_HEALTH_FABRIC_PORT,
                                                XPUM_HEALTH_FREQUENCY};
    if (componentType >= 1 && componentType <= (int)(types.size())) {
        xpum_health_type_t targetType = types[componentType - 1];
        types.clear();
        types.push_back(targetType);
    }
    (*json)["device_id"] = deviceId;
    for (auto& type : types) {
        auto componentJson = (*getHealth(deviceId, type));
        if (componentJson.contains("error")) {
            auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*errorJson)["error"] = componentJson["error"];
            (*errorJson)["errno"] = componentJson["errno"];
            return errorJson;
        }
        std::string currentHealthType = healthTypeEnumToString(type);
        (*json)[currentHealthType]["status"] = componentJson["status"];
        (*json)[currentHealthType]["description"] = componentJson["description"];
        if (componentJson.contains("custom_threshold")) {
            (*json)[currentHealthType]["custom_threshold"] = componentJson["custom_threshold"];
        }
        if (componentJson.contains("throttle_threshold")) {
            (*json)[currentHealthType]["throttle_threshold"] = componentJson["throttle_threshold"];
        }
        if (componentJson.contains("shutdown_threshold")) {
            (*json)[currentHealthType]["shutdown_threshold"] = componentJson["shutdown_threshold"];
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getHealth(int deviceId, xpum_health_type_t type) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_health_data_t data;
    xpum_result_t res = xpumGetHealth(deviceId, type, &data);
    if (res == XPUM_OK) {
        (*json)["type"] = healthTypeEnumToString(data.type);
        (*json)["status"] = healthStatusEnumToString(data.status);
        (*json)["description"] = data.description;
        (*json) = appendHealthThreshold(deviceId, (*json), data.type, data.throttleThreshold, data.shutdownThreshold);
    } else {
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
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setHealthConfig(int deviceId, int cfgtype, int threshold) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = xpumSetHealthConfig(deviceId, static_cast<xpum_health_config_type_t>(cfgtype), &threshold);
    if (res != XPUM_OK) {
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
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getHealthByGroup(uint32_t groupId, int componentType) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setHealthConfigByGroup(uint32_t groupId, int cfgtype, int threshold) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicy() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPolicyById(bool isDevice, uint32_t id) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicyType() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicyConditionType() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicyActionType() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setPolicy(bool isDevcie, uint32_t id, PolicyData& policy) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPolicy(bool isDevcie, uint32_t id) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::string eccStateToString(xpum_ecc_state_t state) {
    if (state == XPUM_ECC_STATE_UNAVAILABLE) {
        return "";
    }
    if (state == XPUM_ECC_STATE_ENABLED) {
        return "enabled";
    }
    if (state == XPUM_ECC_STATE_DISABLED) {
        return "disabled";
    }
    return "";
}

std::string eccActionToString(xpum_ecc_action_t action) {
    if (action == XPUM_ECC_ACTION_NONE) {
        return "none";
    }
    if (action == XPUM_ECC_ACTION_WARM_CARD_RESET) {
        return "warm card reset";
    }
    if (action == XPUM_ECC_ACTION_COLD_CARD_RESET) {
        return "cold card reset";
    }
    if (action == XPUM_ECC_ACTION_COLD_SYSTEM_REBOOT) {
        return "cold system reboot";
    }
    return "none";
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceConfig(int deviceId, int tileId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    xpum_device_properties_t properties;
    std::vector<uint32_t> tileList;
    uint32_t subdevice_Id = tileId;
    int tileCount = -1;
    uint32_t tileTotalCount = 0;

    if (tileId != -1) {
        res = validateDeviceIdAndTileId(deviceId, subdevice_Id);
    } else {
        res = validateDeviceId(deviceId);
    }
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }

    res = xpumGetDeviceProperties(deviceId, &properties);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    
    for (int i = 0; i < properties.propertyLen; i++) {
        auto& prop = properties.properties[i];
        if (prop.name != XPUM_DEVICE_PROPERTY_NUMBER_OF_TILES) {
            continue;
        }
        tileTotalCount = atoi(prop.value);
        break;
    }

    if (tileId != -1) {
        if (subdevice_Id >= tileTotalCount) {
            tileCount = 0;
        } else {
            tileList.push_back(subdevice_Id);
            tileCount = 1;
        }
    } else {
        for (uint32_t i = 0; i < tileTotalCount; i++) {
            tileList.push_back(i);
            tileCount = tileTotalCount;
        }
    }

    xpum_power_limits_t powerLimits;
    res = xpumGetDevicePowerLimits(deviceId, 0, &powerLimits);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }

    int32_t power = powerLimits.sustained_limit.power / 1000;
    bool available;
    bool configurable;
    xpum_ecc_state_t current, pending;
    xpum_ecc_action_t action;

    res = xpumGetEccState(deviceId, &available, &configurable, &current, &pending, &action);

    xpum_frequency_range_t freqArray[32];
    xpum_standby_data_t standbyArray[32];
    xpum_scheduler_data_t schedulerArray[32];
    xpum_power_prop_data_t powerRangeArray[32];
    xpum_device_performancefactor_t performanceFactorArray[32];
    xpum_fabric_port_config_t portConfig[32];
    double availableClocksArray[255];

    uint32_t freqCount = 32;
    uint32_t standbyCount = 32;
    uint32_t schedulerCount = 32;
    uint32_t powerRangeCount = 32;
    uint32_t performanceFactorCount = 32;
    uint32_t portConfigCount = 32;
    uint32_t clockCount = 255;

    res = xpumGetDeviceFrequencyRanges(deviceId, freqArray, &freqCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    res = xpumGetDeviceStandbys(deviceId, standbyArray, &standbyCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    res = xpumGetDeviceSchedulers(deviceId, schedulerArray, &schedulerCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    res = xpumGetDevicePowerProps(deviceId, powerRangeArray, &powerRangeCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    res = xpumGetPerformanceFactor(deviceId, performanceFactorArray, &performanceFactorCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    res = xpumGetFabricPortConfig(deviceId, portConfig, &portConfigCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }

    (*json)["device_id"] = deviceId;
    (*json)["power_limit"] = power;

    for (uint32_t i = 0; i < powerRangeCount; i++) {
        if (powerRangeArray[i].on_subdevice == false) {
            std::string scope = "1 to " + std::to_string(powerRangeArray[i].max_limit / 1000);
            (*json)["power_vaild_range"] = scope;
            break;
        }
    }

    std::vector<nlohmann::json> tileJsonList;
    for (int j{0}; j < tileCount; ++j) {
        uint32_t tileId = tileList.at(j);
        std::string clockString = "";

        auto tileJson = nlohmann::json();
        tileJson["tile_id"] = std::to_string(deviceId) + "/" + std::to_string(tileId);
        for (uint32_t i = 0; i < freqCount; i++) {
            if (freqArray[i].type == XPUM_GPU_FREQUENCY && freqArray[i].subdevice_Id == tileId) {
                tileJson["min_frequency"] = int(freqArray[i].min);
                tileJson["max_frequency"] = int(freqArray[i].max);
                break;
            }
        }
        for (uint32_t i = 0; i < standbyCount; i++) {
            if (standbyArray[i].type == XPUM_GLOBAL && standbyArray[i].subdevice_Id == tileId) {
                tileJson["standby_mode"] = standbyModeToString(standbyArray[i].mode);
                break;
            }
        }
        for (uint32_t i = 0; i < schedulerCount; i++) {
            if (schedulerArray[i].subdevice_Id == tileId) {
                tileJson["scheduler_mode"] = schedulerModeToString(schedulerArray[i].mode);
                if (schedulerArray[i].mode == XPUM_TIMEOUT) {
                    tileJson["scheduler_watchdog_timeout"] = schedulerArray[i].val1;
                } else if (schedulerArray[i].mode == XPUM_TIMESLICE) {
                    tileJson["scheduler_timeslice_interval"] = schedulerArray[i].val1;
                    tileJson["scheduler_timeslice_yield_timeout"] = schedulerArray[i].val2;
                }
                break;
            }
        }
        xpumGetFreqAvailableClocks(deviceId, tileId, availableClocksArray, &clockCount);
        for (uint32_t i = 0; i < clockCount; i++) {
            clockString += std::to_string(std::lround(availableClocksArray[i]));
            if (i < clockCount - 1) {
                clockString += ", ";
            }
        }
        tileJson["gpu_frequency_valid_options"] = clockString;
        tileJson["standby_mode_valid_options"] = "default, never";
        for (uint32_t i = 0; i < performanceFactorCount; i++) {
            if (performanceFactorArray[i].subdevice_id == tileId) {
                if (performanceFactorArray[i].engine == XPUM_COMPUTE) {
                    tileJson["compute_performance_factor"] = int(performanceFactorArray[i].factor);
                }
                if (performanceFactorArray[i].engine == XPUM_MEDIA) {
                    tileJson["media_performance_factor"] = int(performanceFactorArray[i].factor);
                }
            }
        }
        tileJson["compute_engine"] = "compute";
        tileJson["media_engine"] = "media";

        std::string enabled_str = "";
        std::string disabled_str = "";
        std::string beaconing_on_str = "";
        std::string beaconing_off_str = "";
        for (uint32_t i = 0; i < portConfigCount; i++) {
            if (portConfig[i].subdeviceId == tileId) {
                std::string id_str = std::to_string(portConfig[i].portNumber);
                if (portConfig[i].enabled == true) {
                    if (enabled_str.empty()) {
                        enabled_str = id_str;
                    } else {
                        enabled_str += ", " + id_str;
                    }
                } else {
                    if (disabled_str.empty()) {
                        disabled_str = id_str;
                    } else {
                        disabled_str += ", " + id_str;
                    }
                }
                if (portConfig[i].beaconing == true) {
                    if (beaconing_on_str.empty()) {
                        beaconing_on_str = id_str;
                    } else {
                        beaconing_on_str += ", " + id_str;
                    }
                } else {
                    if (beaconing_off_str.empty()) {
                        beaconing_off_str = id_str;
                    } else {
                        beaconing_off_str += ", " + id_str;
                    }
                }
            }
        }
        tileJson["port_up"] = enabled_str;
        tileJson["port_down"] = disabled_str;
        tileJson["beaconing_on"] = beaconing_on_str;
        tileJson["beaconing_off"] = beaconing_off_str;
        (*json)["memory_ecc_current_state"] = eccStateToString(current);
        (*json)["memory_ecc_pending_state"] = eccStateToString(pending);
        tileJsonList.push_back(tileJson);
    }
    (*json)["tile_config_data"] = tileJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceSchedulerMode(int deviceId, int tileId, int mode, int val1, int val2) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = XPUM_GENERIC_ERROR;

    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }

    if (mode == 0) {
        xpum_scheduler_timeout_t sch_timeout;
        sch_timeout.subdevice_Id = tileId;
        sch_timeout.watchdog_timeout = val1;
        if (val1 < 5000 || val1 > 100000000) {
            (*json)["error"] = "Invalid scheduler timeout value";
            (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
            goto LOG_ERR;
        }
        res = xpumSetDeviceSchedulerTimeoutMode(deviceId, sch_timeout);
    } else if (mode == 1) {
        xpum_scheduler_timeslice_t sch_timeslice;
        sch_timeslice.subdevice_Id = tileId;
        sch_timeslice.interval = val1;
        sch_timeslice.yield_timeout = val2;
        if (val1 < 5000 || val1 > 100000000 || val2 < 5000 || val2 > 100000000) {
            (*json)["error"] = "Invalid scheduler timeslice value";
            (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
            goto LOG_ERR;
        }
        res = xpumSetDeviceSchedulerTimesliceMode(deviceId, sch_timeslice);
    } else if (mode == 2) {
        xpum_scheduler_exclusive_t sch_exclusive;
        sch_exclusive.subdevice_Id = tileId;
        res = xpumSetDeviceSchedulerExclusiveMode(deviceId, sch_exclusive);
    } else if (mode == 3) {
        xpum_scheduler_debug_t sch_debug;
        sch_debug.subdevice_Id = tileId;
        res = xpumSetDeviceSchedulerDebugMode(deviceId, sch_debug);
    } else {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
    }
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_GROUP_NOT_FOUND:
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "not support this scheduler mode";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        goto LOG_ERR;
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set scheduler mode %d,%d,%d", mode, val1, val2);
        return json;
    }
LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set scheduler mode %d,%s", mode,
                   (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDevicePowerlimit(int deviceId, int tileId, int power, int interval) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    xpum_power_sustained_limit_t sustained_limit;
    xpum_power_prop_data_t powerRangeArray[32];
    uint32_t powerRangeCount = 32;
    uint32_t pwr_mW = power * 1000;

    res = xpumGetDevicePowerProps(deviceId, powerRangeArray, &powerRangeCount);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        goto LOG_ERR;
    }

    for (uint32_t i = 0; i < powerRangeCount; i++) {
        if (powerRangeArray[i].subdevice_Id == (uint32_t)tileId || tileId == -1) {
            if (pwr_mW < 1 || (uint32_t(powerRangeArray[i].default_limit) > 0  && pwr_mW > uint32_t(powerRangeArray[i].default_limit))) {
                (*json)["error"] = "Invalid power limit value";
                (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
                goto LOG_ERR;
            }
        }
    }
    sustained_limit.enabled = true;
    sustained_limit.power = pwr_mW;
    sustained_limit.interval = interval;
    
    res = xpumSetDevicePowerSustainedLimits(deviceId, tileId, sustained_limit);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set power limit %d,%d", power, interval);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set power limit %s", (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceStandby(int deviceId, int tileId, int mode) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }
    xpum_standby_data_t standby;
    standby.on_subdevice = true;
    standby.subdevice_Id = tileId;
    standby.type = XPUM_GLOBAL;

    if (mode == XPUM_DEFAULT) {
        standby.mode = XPUM_DEFAULT;
    } else if (mode == XPUM_NEVER) {
        standby.mode = XPUM_NEVER;
    } else {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
        goto LOG_ERR;
    }

    res = xpumSetDeviceStandby(deviceId, standby);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Access denied due to permission level or operation unsupported.";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set standby mode %d", mode);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set standby mode %s", (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }
 
    xpum_frequency_range_t freq_range;
    freq_range.subdevice_Id = tileId;
    freq_range.type = XPUM_GPU_FREQUENCY;
    freq_range.min = minFreq;
    freq_range.max = maxFreq;

    res = xpumSetDeviceFrequencyRange(deviceId, freq_range);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set frequency range %d,%d", minFreq, maxFreq);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set frequency range %s", (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::resetDevice(int deviceId, bool force) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    res = validateDeviceId(deviceId);
    if (res != XPUM_OK) {
        (*json)["error"] = "device Id or tile Id is invalid";
        (*json)["errno"] = errorNumTranslate(res);
        goto LOG_ERR;
    }

    res = xpumResetDevice(deviceId, force);
    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND || res == XPUM_RESULT_TILE_NOT_FOUND) {
            (*json)["error"] = "device Id or tile Id is invalid";
        } else if (res == XPUM_UPDATE_FIRMWARE_TASK_RUNNING){
            (*json)["error"] = "device is updating firmware";
        } else if (res == XPUM_RESULT_RESET_FAIL) {
            (*json)["error"] = "Fail to reset device";
        } else {
            (*json)["error"] = "Error";
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to reset device with force == %d", force);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to reset device with force == %d, %s", force, (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPerformanceFactor(int deviceId, int tileId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        XPUM_LOG_AUDIT("Fail to get performance factor, %s", 
            (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
        return json;
    }
    uint32_t count = 0;
    res = xpumGetPerformanceFactor(deviceId, nullptr, &count);
    if (res != XPUM_OK) {
        (*json)["error"] = "Error";
        (*json)["errno"] = errorNumTranslate(res);
        XPUM_LOG_AUDIT("Fail to get performance factor, %s", 
            (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
        return json;
    }

    xpum_device_performancefactor_t dataArray[count];
    res = xpumGetPerformanceFactor(deviceId, dataArray, &count);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        XPUM_LOG_AUDIT("Fail to get performance factor, %s", 
            (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    } else {
        std::vector<nlohmann::json> pfList;
        for (uint32_t i = 0; i < count; i++) {
            if (dataArray[i].subdevice_id == (unsigned int)tileId) {
                auto pr = nlohmann::json();
                pr["tile_id"] = dataArray[i].subdevice_id;
                pr["engine"] = dataArray[i].engine;
                pr["factor"] = dataArray[i].factor;
                pfList.push_back(pr);
            }
        }
        (*json)["status"] = "OK";
        (*json)["performance_factor_list"] = pfList;
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setPerformanceFactor(int deviceId, int tileId, xpum_engine_type_flags_t engine, double factor) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }

    xpum_device_performancefactor_t pf;
    pf.on_subdevice = true;
    pf.subdevice_id = tileId;
    pf.engine = engine;
    pf.factor = factor;

    res = xpumSetPerformanceFactor(deviceId, pf);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set performance factor %d,%f", engine, factor);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set performance factor %s", (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setFabricPortEnabled(int deviceId, int tileId, uint32_t port, uint32_t enabled) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }
    xpum_fabric_port_config_t portConfig;
    portConfig.onSubdevice = true;
    portConfig.subdeviceId = tileId;
    portConfig.portNumber = uint8_t(port);
    portConfig.setting_enabled = true;
    portConfig.setting_beaconing = false;
    portConfig.enabled = enabled;

    res = xpumSetFabricPortConfig(deviceId, portConfig);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set fabric port Enabled %d,%d", port, enabled);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set fabric port Enabled %s", (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setFabricPortBeaconing(int deviceId, int tileId, uint32_t port, uint32_t beaconing) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }

    xpum_fabric_port_config_t portConfig;
    portConfig.onSubdevice = true;
    portConfig.subdeviceId = tileId;
    portConfig.portNumber = uint8_t(port);
    portConfig.setting_enabled = false;
    portConfig.setting_beaconing = true;
    portConfig.beaconing = beaconing;

    res = xpumSetFabricPortConfig(deviceId, portConfig);
    if (res != XPUM_OK) { 
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
            case XPUM_RESULT_TILE_NOT_FOUND:
                (*json)["error"] = "device Id or tile Id is invalid";
                break;
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    } else {
        (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set fabric port Beaconing %d,%d", port, beaconing);
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Fail to set fabric port Beaconing %s", (*json)["error"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setMemoryEccState(int deviceId, bool enabled) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    bool available;
    bool configurable;
    xpum_ecc_state_t current, pending;
    xpum_ecc_action_t action;
    xpum_ecc_state_t newState;
    
    if (enabled == true) {
        newState = XPUM_ECC_STATE_ENABLED;
    } else {
        newState = XPUM_ECC_STATE_DISABLED;
    }

    res = xpumSetEccState(deviceId, newState, &available, &configurable, &current, &pending, &action);
    if (available == true) {
        (*json)["memory_ecc_available"] = "true";
    } else {
        (*json)["memory_ecc_available"] = "false";
    }
    if (configurable == true) {
        (*json)["memory_ecc_configurable"] = "true";
    } else {
        (*json)["memory_ecc_configurable"] = "false";
    }
    (*json)["memory_ecc_current_state"] = eccStateToString(current);
    (*json)["memory_ecc_pending_state"] = eccStateToString(pending);
    (*json)["memory_ecc_pending_action"] = eccActionToString(action);

    if (res != XPUM_OK) {
        if (res == XPUM_RESULT_DEVICE_NOT_FOUND || res == XPUM_RESULT_TILE_NOT_FOUND) {
            (*json)["error"] = "device Id or tile Id is invalid";
        } else if (res == XPUM_RESULT_MEMORY_ECC_LIB_NOT_SUPPORT){
            (*json)["error"] = "Failed to " + (enabled ? std::string("enable") : std::string("disable")) +" ECC memory on GPU " + std::to_string(deviceId)+ ". This feature requires the igsc-0.8.3 library or newer. Please check the installation instructions on how to install or update to the latest igsc version.";
        } else {
            (*json)["error"] = "Error Failed to set memory Ecc state: available: " + std::string((*json)["memory_ecc_available"]) +
                ", configurable: " + std::string((*json)["memory_ecc_configurable"]) +
                ", current: " + std::string((*json)["memory_ecc_current_state"]) +
                ", pending: " + std::string((*json)["memory_ecc_pending_state"]) +
                ", action: " + std::string((*json)["memory_ecc_pending_action"]);
        }
        (*json)["errno"] = errorNumTranslate(res);
        goto LOG_ERR;
    } else {
       (*json)["status"] = "OK";
        XPUM_LOG_AUDIT("Succeed to set memory ECC state: available: %s, configurable: %s, current: %s, pending: %s, action: %s",
                        (*json)["memory_ecc_available"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_configurable"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                        (*json)["memory_ecc_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_pending_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                        (*json)["memory_ecc_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
        return json;
    }

LOG_ERR:
    XPUM_LOG_AUDIT("Failed to set memory ECC state: available: %s, configurable: %s, current: %s, pending: %s, action: %s",
                    (*json)["memory_ecc_available"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_configurable"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                    (*json)["memory_ecc_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_pending_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                    (*json)["memory_ecc_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceProcessState(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res;
    uint32_t count;

    res = xpumGetDeviceProcessState(deviceId, nullptr, &count);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    if (count > 0) {
        xpum_device_process_t dataArray[count];
        res = xpumGetDeviceProcessState(deviceId, dataArray, &count);
        if (res != XPUM_OK) {
            switch (res) {
                case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                    (*json)["error"] = "Level Zero Initialization Error";
                    break;
                default:
                    (*json)["error"] = "Error";
                    break;
            }
            (*json)["errno"] = errorNumTranslate(res);
        } else {
            std::vector<nlohmann::json> deviceProcessList;
            for (uint32_t i = 0; i < count; i++) {
                auto proc = nlohmann::json();
                proc["process_id"] = dataArray[i].processId;
                proc["process_name"] = dataArray[i].processName;
                deviceProcessList.push_back(proc);
            }
            (*json)["device_process_list"] = deviceProcessList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceComponentOccupancyRatio(int deviceId, int tileId, int samplingInterval) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceUtilizationByProcess(
        int deviceId, int utilizationInterval) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    uint32_t count = 1024;
    xpum_device_util_by_process_t dataArray[count];
    xpum_result_t res = xpumGetDeviceUtilizationByProcess(
            deviceId, utilizationInterval, dataArray, &count);
    if (res == XPUM_OK) {
        std::vector<nlohmann::json> utils;
        for (uint32_t i = 0; i < count; i++) {
            auto util = nlohmann::json();
            util["process_id"] = dataArray[i].processId;
            util["process_name"] = dataArray[i].processName;
            util["device_id"] = dataArray[i].deviceId;
            util["mem_size"] = dataArray[i].memSize / 1000;
            util["shared_mem_size"] = dataArray[i].sharedMemSize / 1000;
            utils.push_back(util);
        }
        (*json)["device_util_by_proc_list"] = utils;
    } else {
        switch (res) {
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "Device not found";
                break;
            case XPUM_BUFFER_TOO_SMALL:
                (*json)["error"] = "Buffer is too small";
                break;
            case XPUM_INTERVAL_INVALID:
                (*json)["error"] = "Interval must be (0, 1000*1000]";
                break;
            default:
                (*json)["error"] = "Error";
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllDeviceUtilizationByProcess(
        int utilizationInterval) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    uint32_t count = 1024 * 4;
    xpum_device_util_by_process_t dataArray[count];
    xpum_result_t res = xpumGetAllDeviceUtilizationByProcess(
            utilizationInterval, dataArray, &count);
    if (res == XPUM_OK) {
        std::vector<nlohmann::json> utils;
        for (uint32_t i = 0; i < count; i++) {
            auto util = nlohmann::json();
            util["process_id"] = dataArray[i].processId;
            util["process_name"] = dataArray[i].processName;
            util["device_id"] = dataArray[i].deviceId;
            util["mem_size"] = dataArray[i].memSize / 1000;
            util["shared_mem_size"] = dataArray[i].sharedMemSize / 1000;
            utils.push_back(util);
        }
        (*json)["device_util_by_proc_list"] = utils;
    } else {
        switch (res) {
            case XPUM_BUFFER_TOO_SMALL:
                (*json)["error"] = "Buffer is too small";
                break;
            case XPUM_INTERVAL_INVALID:
                (*json)["error"] = "Interval must be (0, 1000*1000]";
                break;
            default:
                (*json)["error"] = "Error";
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::string LibCoreStub::getTopoXMLBuffer() {
    int size = 0;
    std::string result;
    xpum_result_t res = xpumExportTopology2XML(nullptr, &size);
    if (res == XPUM_OK) {
        std::shared_ptr<char> newBuffer(static_cast<char*>(malloc(size)), free);
        res = xpumExportTopology2XML(newBuffer.get(), &size);
        if (res == XPUM_OK) {
            result = newBuffer.get();
        }
    }
    return result;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getXelinkTopology() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_xelink_topo_info* topoInfo;
    int count{1024};
    xpum_xelink_topo_info xelink_topo[count];
    topoInfo = xelink_topo;
    xpum_result_t res = xpumGetXelinkTopology(xelink_topo, &count);
    if (res == XPUM_BUFFER_TOO_SMALL) {
        xpum_xelink_topo_info xelink_topo[count];
        topoInfo = xelink_topo;
        res = xpumGetXelinkTopology(xelink_topo, &count);
    }
    if (res == XPUM_OK) {
        std::vector<nlohmann::json> topoJsonList;
        for (int i{0}; i < count; ++i) {
            auto componentJson = nlohmann::json();
            componentJson["local_device_id"] = topoInfo[i].localDevice.deviceId;
            componentJson["local_on_subdevice"] = topoInfo[i].localDevice.onSubdevice;
            componentJson["local_subdevice_id"] = topoInfo[i].localDevice.subdeviceId;
            componentJson["local_numa_index"] = topoInfo[i].localDevice.numaIdx;
            componentJson["local_cpu_affinity"] = topoInfo[i].localDevice.cpuAffinity;
            componentJson["remote_device_id"] = topoInfo[i].remoteDevice.deviceId;
            componentJson["remote_subdevice_id"] = topoInfo[i].remoteDevice.subdeviceId;
            std::string linkType;
            if (topoInfo[i].linkType == XPUM_LINK_SELF) {
                linkType = "S";
            } else if (topoInfo[i].linkType == XPUM_LINK_MDF) {
                linkType = "MDF";
            } else if (topoInfo[i].linkType == XPUM_LINK_XE) {
                linkType = "XL";
                std::vector<uint32_t> portList;
                for (int n = 0; n < XPUM_MAX_XELINK_PORT; n++) {
                    uint32_t value = topoInfo[i].linkPorts[n];
                    portList.push_back(value);
                }
                componentJson["port_list"] = portList;
            } else if (topoInfo[i].linkType == XPUM_LINK_SYS) {
                linkType = "SYS";
            } else if (topoInfo[i].linkType == XPUM_LINK_NODE) {
                linkType = "NODE";
            } else if (topoInfo[i].linkType == XPUM_LINK_XE_TRANSMIT) {
                linkType = "XL*";
            } else {
                linkType = "Unknown";
            }
            componentJson["link_type"] = linkType;
            topoJsonList.push_back(componentJson);
        }
        (*json)["topo_list"] = topoJsonList;
    }

    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    }

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::runStress(int deviceId, uint32_t stressTime) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = xpumRunStress(deviceId, stressTime);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            case XPUM_RESULT_DIAGNOSTIC_TASK_NOT_COMPLETE:
                (*json)["error"] = 
                    "last stress task on the device is not completed";
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "device not found";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::checkStress(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    int count = XPUM_MAX_NUM_DEVICES;
    xpum_diag_task_info_t taskInfos[XPUM_MAX_NUM_DEVICES]; 
    xpum_result_t res = xpumCheckStress(deviceId, taskInfos, &count);
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    std::vector<nlohmann::json> tasks;
    for (int i = 0; i < count; i++) {
        auto taskJson = nlohmann::json();
        taskJson["device_id"] = taskInfos[i].deviceId;
        taskJson["finished"] = taskInfos[i].finished;
        tasks.push_back(taskJson);
    }
    (*json)["task_list"] = tasks;
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::genDebugLog(const std::string &fileName) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = xpumGenerateDebugLog(fileName.c_str());
    if (res == XPUM_OK) {
        (*json)["status"] = "OK";
    } else {
        if (res == XPUM_RESULT_FILE_DUP) {
            (*json)["error"] = "Duplicated File Name Error";
        } else if (res == XPUM_RESULT_INVALID_DIR) {
            (*json)["error"] = "Invalid Directory Error";
        } else {
            (*json)["error"] = "Error";
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}


std::string LibCoreStub::getPciSlotName(std::vector<std::string> &bdfs) {
    uint32_t sizePciPath = bdfs.size();
    char *pciPath[sizePciPath];
    uint32_t sizeName = 2048;
    char name[sizeName];

    for (uint32_t i = 0; i < sizePciPath; i++) {
        pciPath[i] = (char *)bdfs[i].c_str();
    }

    xpum_result_t res = ::getPciSlotName(
            pciPath, sizePciPath, name, sizeName);
    if (res == XPUM_OK) {
        return std::string(name);
    } else {
        return "";
    }
}

std::unique_ptr<nlohmann::json> LibCoreStub::doVgpuPrecheck() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_vgpu_precheck_result_t precheckResult;
    xpum_result_t res = xpumDoVgpuPrecheck(&precheckResult);
    if (res == XPUM_OK) {
        (*json)["vmx_flag"] = precheckResult.vmxFlag ? "Pass" : "Fail";
        (*json)["vmx_message"] = precheckResult.vmxMessage;
        (*json)["iommu_status"] = precheckResult.iommuStatus ? "Pass": "Fail";
        (*json)["iommu_message"] = precheckResult.iommuMessage;
        (*json)["sriov_status"] = precheckResult.sriovStatus ? "Pass": "Fail";
        (*json)["sriov_message"] = precheckResult.sriovMessage;
    } else {
        (*json)["error"] = "Error";
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::createVf(int deviceId, uint32_t numVfs, uint64_t lmem) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_vgpu_config_t config{numVfs, lmem};
    xpum_result_t res = xpumCreateVf(deviceId, &config);
    if (res != XPUM_OK) {
        if (res == XPUM_VGPU_INVALID_LMEM) {
            (*json)["error"] = "Invalid virtual GPU local memory";
        } else if (res == XPUM_VGPU_INVALID_NUMVFS) {
            (*json)["error"] = "Invalid number of virtual GPUs";
        } else if (res == XPUM_VGPU_DIRTY_PF) {
            (*json)["error"] = "Please clear virtual GPUs first";
        } else if (res == XPUM_VGPU_VF_UNSUPPORTED_OPERATION) {
            (*json)["error"] = "Do not creating virtual GPUs on virtual device";
        } else if (res == XPUM_VGPU_CREATE_VF_FAILED) {
            (*json)["error"] = "Fail to create virtual GPUs";
        } else if (res == XPUM_VGPU_NO_CONFIG_FILE) {
            (*json)["error"] = "vGPU configuration file doesn't exist";
        } else if (res == XPUM_VGPU_SYSFS_ERROR) {
            (*json)["error"] = "Error in sysfs";
        } else if (res == XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL) {
            (*json)["error"] = "Unsupported device model";
        } else {
            (*json)["error"] = "Error";
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceFunction(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_vgpu_function_info_t resList[XPUM_MAX_VF_NUM];
    int count = XPUM_MAX_VF_NUM;
    xpum_result_t res = xpumGetDeviceFunctionList(deviceId, resList, &count);
    if (res == XPUM_OK) {
        std::vector<nlohmann::json> vfs;
        for (int i = 0; i < count; i++) {
            auto vfInfo = nlohmann::json();
            vfInfo["bdf_address"] = resList[i].bdfAddress;
            vfInfo["lmem_size"] = resList[i].lmemSize;
            vfInfo["function_type"] = deviceFunctionTypeEnumToString(resList[i].functionType);
            vfInfo["device_id"] = resList[i].deviceId >= 0 ? std::to_string(resList[i].deviceId) : "";
            vfs.push_back(vfInfo);
        }
        (*json)["vf_list"] = vfs;
    } else {
        if (res == XPUM_VGPU_SYSFS_ERROR) {
            (*json)["error"] = "Error in sysfs";
        } else if (res == XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL) {
            (*json)["error"] = "Unsupported device model";
        } else {
            (*json)["error"] = "Error";
        } 
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::removeAllVf(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = xpumRemoveAllVf(deviceId);
    if (res == XPUM_OK) {
        (*json)["status"] = "OK";
    } else {
        if (res == XPUM_VGPU_REMOVE_VF_FAILED) {
            (*json)["error"] = "Fail to remove all virtual GPUs";
        } else if (res == XPUM_VGPU_SYSFS_ERROR) {
            (*json)["error"] = "Error in sysfs";
        } else if (res == XPUM_VGPU_UNSUPPORTED_DEVICE_MODEL) {
            (*json)["error"] = "Unsupported device model";
        } else {
            (*json)["error"] = "Error";
        }
        (*json)["errno"] = errorNumTranslate(res);
    }
    return json;
}

} // end namespace xpum::cli
