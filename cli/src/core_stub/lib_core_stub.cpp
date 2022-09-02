/* 
 *  Copyright (C) 2021-2022 Intel Corporation
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

using namespace xpum;

namespace xpum::cli {

LibCoreStub::LibCoreStub() {
    putenv(const_cast<char *>("SPDLOG_LEVEL=OFF"));
    xpumInit();
}

LibCoreStub::~LibCoreStub() {
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
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupName name;
    name.set_name(groupName);
    grpc::Status status = stub->groupCreate(&context, name, &response);
    if (status.ok() && response.errormsg().length() == 0) {
        XPUM_LOG_AUDIT("Succeed to create group %d,%s", response.id(), groupName.c_str());
        (*json)["group_id"] = response.id();
        (*json)["group_name"] = response.groupname();
        (*json)["device_count"] = response.count();

        std::vector<int32_t> deviceIdList;
        for (uint32_t j{0}; j < response.count(); ++j) {
            deviceIdList.push_back(response.devicelist(j).id());
        }

        (*json)["device_id_list"] = deviceIdList;

    } else {
        XPUM_LOG_AUDIT("Fail to create group %s", groupName.c_str());
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupDelete(int groupId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupId id;
    id.set_id(groupId);
    grpc::Status status = stub->groupDestory(&context, id, &response);
    if (status.ok() && response.errormsg().length() == 0) {
        (*json)["group_id"] = response.id();
        XPUM_LOG_AUDIT("Succeed to delete group %d", groupId);
    } else {
        (*json)["error"] = response.errormsg();
        XPUM_LOG_AUDIT("Fail to delete group %d", groupId);
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupListAll() {
    using namespace xpum;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    xpum_group_id_t groups[XPUM_MAX_NUM_GROUPS];
    int count = XPUM_MAX_NUM_GROUPS;
    xpum_result_t res = xpumGetAllGroupIds(groups, &count);
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
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupId id;
    id.set_id(groupId);
    grpc::Status status = stub->groupGetInfo(&context, id, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["group_id"] = response.id();
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<int32_t> deviceIdList;
            for (uint32_t j{0}; j < response.count(); ++j) {
                deviceIdList.push_back(response.devicelist(j).id());
            }

            (*json)["device_id_list"] = deviceIdList;

        } else {
            (*json)["error message"] = response.errormsg();
        }
    } else {
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupAddDevice(int groupId, int deviceId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupAddRemoveDevice groupAR;
    groupAR.set_groupid(groupId);
    groupAR.set_deviceid(deviceId);
    grpc::Status status = stub->groupAddDevice(&context, groupAR, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            XPUM_LOG_AUDIT("Succeed to add device(%d) to group %d", deviceId, groupId);
            (*json)["group_id"] = groupId;
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<int32_t> deviceIdList;
            for (uint32_t j{0}; j < response.count(); ++j) {
                deviceIdList.push_back(response.devicelist(j).id());
            }

            (*json)["device_id_list"] = deviceIdList;

        } else {
            XPUM_LOG_AUDIT("Fail to add device(%d) to group %d", deviceId, groupId);
            (*json)["device_id"] = deviceId;
            (*json)["error"] = response.errormsg();
        }
    } else {
        XPUM_LOG_AUDIT("Fail to add device(%d) to group %d", deviceId, groupId);
        (*json)["device_id"] = deviceId;
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::groupRemoveDevice(int groupId, int deviceId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupAddRemoveDevice groupAR;
    groupAR.set_groupid(groupId);
    groupAR.set_deviceid(deviceId);
    grpc::Status status = stub->groupRemoveDevice(&context, groupAR, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            XPUM_LOG_AUDIT("Succeed to remove device(%d) from group %d", deviceId, groupId);
            (*json)["group_id"] = groupId;
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<int32_t> deviceIdList;
            for (uint32_t j{0}; j < response.count(); ++j) {
                deviceIdList.push_back(response.devicelist(j).id());
            }

            (*json)["device_id_list"] = deviceIdList;
        } else {
            XPUM_LOG_AUDIT("Fail to remove device(%d) from group %d", deviceId, groupId);
            (*json)["device_id"] = deviceId;
            (*json)["error"] = response.errormsg();
        }
    } else {
        XPUM_LOG_AUDIT("Fail to remove device(%d) from group %d", deviceId, groupId);
        (*json)["device_id"] = deviceId;
        (*json)["error"] = response.errormsg();
    }
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

std::unique_ptr<nlohmann::json> LibCoreStub::runDiagnostics(const char *bdf, int level, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_device_id_t deviceId;
    xpum_result_t res = xpumGetDeviceIdByBDF(bdf, &deviceId);
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
        return json;
    }
    return runDiagnostics(deviceId, level, rawComponentTypeStr);
}

std::unique_ptr<nlohmann::json> LibCoreStub::runDiagnostics(int deviceId, int level, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = xpumRunDiagnostics(deviceId, static_cast<xpum_diag_level_t>(level));
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

std::unique_ptr<nlohmann::json> LibCoreStub::getDiagnosticsResult(const char *bdf, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_device_id_t deviceId;
    xpum_result_t res = xpumGetDeviceIdByBDF(bdf, &deviceId);
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
        return json;
    }
    return getDiagnosticsResult(deviceId, rawComponentTypeStr);
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDiagnosticsResult(int deviceId, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_diag_task_info_t task_info;
    xpum_result_t res = xpumGetDiagnosticsResult(deviceId, &task_info);
    if (res == XPUM_OK) {
        (*json)["device_id"] = task_info.deviceId;
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
            if (task_info.componentList[i].type == XPUM_DIAG_SOFTWARE_EXCLUSIVE 
                    && task_info.componentList[i].result == XPUM_DIAG_RESULT_FAIL) {
                uint32_t count = 0;
                res = xpumGetDeviceProcessState(task_info.deviceId, nullptr, &count);
                if (res == XPUM_OK && count > 0) {
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

std::unique_ptr<nlohmann::json> LibCoreStub::runDiagnosticsByGroup(uint32_t groupId, int level, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

nlohmann::json LibCoreStub::appendHealthThreshold(int deviceId, nlohmann::json json, HealthType type,
                                               uint64_t throttleValue, uint64_t shutdownValue) {
    if (type == HEALTH_POWER) {
        json["custom_threshold"] = getHealthConfig(deviceId, HEALTH_POWER_LIMIT);
        json["throttle_threshold"] = throttleValue;
    }
    if (type == HEALTH_CORE_THERMAL) {
        json["custom_threshold"] = getHealthConfig(deviceId, HEALTH_CORE_THERMAL_LIMIT);
        json["throttle_threshold"] = throttleValue;
        json["shutdown_threshold"] = shutdownValue;
    }
    if (type == HEALTH_MEMORY_THERMAL) {
        json["custom_threshold"] = getHealthConfig(deviceId, HEALTH_MEMORY_THERMAL_LIMIT);
        json["throttle_threshold"] = throttleValue;
        json["shutdown_threshold"] = shutdownValue;
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllHealth() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> healthJsonList;
            for (int i = 0; i < response.info_size(); i++) {
                auto healthJson = (*getHealth(response.info(i).id().id(), -1));
                healthJsonList.push_back(healthJson);
            }
            (*json)["device_list"] = healthJsonList;
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getHealth(int deviceId, int componentType) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthDataRequest request;
    request.set_deviceid(deviceId);
    HealthData response;
    (*json)["device_id"] = deviceId;
    grpc::Status status = grpc::Status::OK;
    std::vector<HealthType> types = {HEALTH_CORE_THERMAL, HEALTH_MEMORY_THERMAL, HEALTH_POWER, HEALTH_MEMORY, HEALTH_FABRIC_PORT};
    if (componentType >= 1 && componentType <= (int)(types.size())) {
        HealthType targetType = types[componentType - 1];
        types.clear();
        types.push_back(targetType);
    }
    for (auto& type : types) {
        auto componentJson = (*getHealth(deviceId, type));
        if (componentJson.contains("error")) {
            auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*errorJson)["error"] = componentJson["error"];
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

std::unique_ptr<nlohmann::json> LibCoreStub::getHealth(int deviceId, HealthType type) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthDataRequest request;
    request.set_deviceid(deviceId);
    request.set_type(type);
    HealthData response;
    grpc::Status status = stub->getHealth(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["type"] = healthTypeEnumToString(response.type());
            (*json)["status"] = healthStatusEnumToString(response.statustype());
            (*json)["description"] = response.description();
            (*json) = appendHealthThreshold(deviceId, (*json), response.type(),
                                            response.throttlethreshold(), response.shutdownthreshold());
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setHealthConfig(int deviceId, HealthConfigType cfgtype, int threshold) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthConfigRequest request;
    request.set_deviceid(deviceId);
    request.set_configtype(cfgtype);
    request.set_threshold(threshold);
    HealthConfigInfo response;
    grpc::Status status = stub->setHealthConfig(&context, request, &response);
    std::string healthTypeStr = healthTypeEnumToString((HealthType)cfgtype);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Failed to set health threshold on device %d type %s threshold %d", deviceId, healthTypeStr.c_str(), threshold);
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Failed to set health threshold on device %d type %s threshold %d", deviceId, healthTypeStr.c_str(), threshold);
    }
    XPUM_LOG_AUDIT("Succeed to set health threshold on device %d type %s threshold %d", deviceId, healthTypeStr.c_str(), threshold);
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getHealthByGroup(uint32_t groupId, int componentType) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    (*json)["group_id"] = groupId;
    HealthDataByGroupRequest request;
    request.set_groupid(groupId);
    HealthDataByGroup response;
    std::vector<nlohmann::json> deviceJsonList;
    std::vector<HealthType> types = {HEALTH_CORE_THERMAL, HEALTH_MEMORY_THERMAL, HEALTH_POWER, HEALTH_MEMORY, HEALTH_FABRIC_PORT};
    if (componentType >= 1 && componentType <= (int)(types.size())) {
        HealthType targetType = types[componentType - 1];
        types.clear();
        types.push_back(targetType);
    }
    for (auto& type : types) {
        auto deviceHealthTypeJsons = (*getHealthByGroup(groupId, type));
        if (deviceHealthTypeJsons.contains("error")) {
            auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
            (*errorJson)["error"] = deviceHealthTypeJsons["error"];
            return errorJson;
        }
        std::string currentHealthType = healthTypeEnumToString(type);
        for (auto& component : deviceHealthTypeJsons[currentHealthType]) {
            std::size_t targetDeviceIndex = deviceJsonList.size();
            for (std::size_t i = 0; i < deviceJsonList.size(); i++) {
                if (deviceJsonList[i]["device_id"] == component["device_id"]) {
                    targetDeviceIndex = i;
                }
            }
            if (targetDeviceIndex == deviceJsonList.size()) {
                auto deviceJson = nlohmann::json();
                deviceJson["device_id"] = component["device_id"];
                deviceJsonList.push_back(deviceJson);
            }
            deviceJsonList[targetDeviceIndex][currentHealthType]["status"] = component["status"];
            deviceJsonList[targetDeviceIndex][currentHealthType]["description"] = component["description"];
            if (component.contains("custom_threshold")) {
                deviceJsonList[targetDeviceIndex][currentHealthType]["custom_threshold"] = component["custom_threshold"];
            }
            if (component.contains("throttle_threshold")) {
                deviceJsonList[targetDeviceIndex][currentHealthType]["throttle_threshold"] = component["throttle_threshold"];
            }
            if (component.contains("shutdown_threshold")) {
                deviceJsonList[targetDeviceIndex][currentHealthType]["shutdown_threshold"] = component["shutdown_threshold"];
            }
        }
    }
    (*json)["device_count"] = deviceJsonList.size();
    (*json)["device_list"] = deviceJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getHealthByGroup(uint32_t groupId, HealthType type) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthDataByGroupRequest request;
    request.set_groupid(groupId);
    request.set_type(type);
    HealthDataByGroup response;
    std::vector<nlohmann::json> componentJsonList;
    grpc::Status status = stub->getHealthByGroup(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            for (int i = 0; i < response.healthdata_size(); i++) {
                auto component = nlohmann::json();
                component["device_id"] = response.healthdata(i).deviceid();
                component["status"] = healthStatusEnumToString(response.healthdata(i).statustype());
                component["description"] = response.healthdata(i).description();
                component = appendHealthThreshold(response.healthdata(i).deviceid(), component, response.type(),
                                                  response.healthdata(i).throttlethreshold(), response.healthdata(i).shutdownthreshold());

                componentJsonList.push_back(component);
            }
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    (*json)[healthTypeEnumToString(type)] = componentJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setHealthConfigByGroup(uint32_t groupId, HealthConfigType cfgtype, int threshold) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthConfigByGroupRequest request;
    request.set_groupid(groupId);
    request.set_configtype(cfgtype);
    request.set_threshold(threshold);
    HealthConfigByGroupInfo response;
    grpc::Status status = stub->setHealthConfigByGroup(&context, request, &response);
    std::string healthTypeStr = healthTypeEnumToString((HealthType)cfgtype);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Failed to set health threshold on group %d type %s threshold %d", groupId, healthTypeStr.c_str(), threshold);
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Failed to set health threshold on group %d type %s threshold %d", groupId, healthTypeStr.c_str(), threshold);
    }
    XPUM_LOG_AUDIT("Succeed to set health threshold on group %d type %s threshold %d", groupId, healthTypeStr.c_str(), threshold);
    return json;
}

int LibCoreStub::getHealthConfig(int deviceId, HealthConfigType cfgtype) {
    int threshold = -1;
    grpc::ClientContext context;
    HealthConfigRequest request;
    request.set_deviceid(deviceId);
    request.set_configtype(cfgtype);
    HealthConfigInfo response;
    grpc::Status status = stub->getHealthConfig(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            threshold = response.threshold();
        }
    }
    return threshold;
}

std::string LibCoreStub::policyTypeEnumToString(XpumPolicyType type) {
    //std::string ret = "POLICY_TYPE_MAX";
    std::string ret = "Error: cli unsupport this type";
    switch (type) {
        case POLICY_TYPE_GPU_TEMPERATURE:
            ret = "1. GPU Core Temperature";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            ret = "2. Programming Errors";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS:
            ret = "3. Driver Errors";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            ret = "4. Cache Errors Correctable";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            ret = "5. Cache Errors Uncorrectable";
            break;
        // case POLICY_TYPE_GPU_MEMORY_TEMPERATURE:
        //     ret = "POLICY_TYPE_GPU_MEMORY_TEMPERATURE";
        //     break;
        // case POLICY_TYPE_GPU_POWER:
        //     ret = "POLICY_TYPE_GPU_POWER";
        //     break;
        // case POLICY_TYPE_RAS_ERROR_CAT_RESET:
        //     ret = "POLICY_TYPE_RAS_ERROR_CAT_RESET";
        //     break;
        // case POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
        //     ret = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE";
        //     break;
        // case POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
        //     ret = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE";
        //     break;
        default:
            break;
    }
    return ret;
}

std::string LibCoreStub::policyConditionTypeEnumToString(XpumPolicyConditionType type) {
    //std::string ret = "POLICY_CONDITION_TYPE_GREATER";
    std::string ret = "1. More than";
    switch (type) {
        case POLICY_CONDITION_TYPE_GREATER:
            ret = "1. More than";
            break;
        case POLICY_CONDITION_TYPE_LESS:
            ret = "3. Less than";
            break;
        case POLICY_CONDITION_TYPE_WHEN_INCREASE:
            ret = "2. When occur";
            break;
        default:
            break;
    }
    return ret;
}

std::string LibCoreStub::policyActionTypeEnumToString(XpumPolicyActionType type) {
    std::string ret = "4. No action";
    switch (type) {
        case POLICY_ACTION_TYPE_NULL:
            ret = "3. Notify";
            break;
        case POLICY_ACTION_TYPE_THROTTLE_DEVICE:
            ret = "1. Throttle GPU Core Frequency";
            break;
        // case POLICY_ACTION_TYPE_RESET_DEVICE:
        //     ret = "2. Reset GPU";
        //     break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicy() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> dataList;
            for (int i = 0; i < response.info_size(); i++) {
                auto healthJson = *getPolicy(true, response.info(i).id().id());
                dataList.push_back(healthJson);
            }
            (*json)["all_policy_list"] = dataList;
        } else {
            (*json)["error"] = response.errormsg();
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPolicyById(bool isDevice, uint32_t id) {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    auto healthJson = (*getPolicy(isDevice, id));
    //std::cout << "--Gang---core_stub.cpp--getPolicyById---1----healthJson=" << healthJson << std::endl;
    if (healthJson.contains("error")) {
        std::string jsonStr = healthJson.dump();
        std::string::size_type position = jsonStr.find("There is no data");
        if (position != jsonStr.npos) {
            std::vector<nlohmann::json> dataList;
            (*json)["all_policy_list"] = dataList;
            return json;
        } else {
            (*json) = healthJson;
            return json;
        }
    }
    (*json)["all_policy_list"] = healthJson;
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicyType() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    //grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    //std::cout << "--Gang---core_stub.cpp--getAllPolicyType---1----status.ok()=" << status.ok() << std::endl;
    //if (status.ok())
    {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> healthJsonList;
            {
                auto component = nlohmann::json();
                component["action"] = "1. Throttle GPU Core";
                component["condition"] = "1. More than";
                component["type"] = "1. GPU Core Temperature";
                healthJsonList.push_back(component);
            }
            // {
            //     auto component = nlohmann::json();
            //     component["action"] = "2. Reset GPU";
            //     component["condition"] = "1. More than";
            //     component["type"] = "2. Programming Errors";
            //     healthJsonList.push_back(component);
            // }
            // {
            //     auto component = nlohmann::json();
            //     component["action"] = "2. Reset GPU";
            //     component["condition"] = "1. More than";
            //     component["type"] = "3. Driver Errors";
            //     healthJsonList.push_back(component);
            // }
            // {
            //     auto component = nlohmann::json();
            //     component["action"] = "2. Reset GPU";
            //     component["condition"] = "1. More than";
            //     component["type"] = "4. Cache Errors Correctable";
            //     healthJsonList.push_back(component);
            // }
            // {
            //     auto component = nlohmann::json();
            //     component["action"] = "2. Reset GPU";
            //     component["condition"] = "2. When occur";
            //     component["type"] = "5. Cache Errors Uncorrectable";
            //     healthJsonList.push_back(component);
            // }

            (*json)["all_policy_type"] = healthJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicyConditionType() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> healthJsonList;
            healthJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
            healthJsonList.push_back("POLICY_CONDITION_TYPE_LESS");
            healthJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");
            (*json)["all_policy_list"] = healthJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllPolicyActionType() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> healthJsonList;
            healthJsonList.push_back("POLICY_ACTION_TYPE_NULL");
            healthJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
            //healthJsonList.push_back("POLICY_ACTION_TYPE_RESET_DEVICE");
            (*json)["all_policy_list"] = healthJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setPolicy(bool isDevcie, uint32_t id, XpumPolicyData& policy) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    SetPolicyRequest request;
    request.set_id(id);
    request.set_isdevcie(isDevcie);
    //request.set_allocated_policy(&policy);
    //Answer* ans = detail->mutable_answer();
    XpumPolicyData* p_policy = request.mutable_policy();
    p_policy->CopyFrom(policy);
    SetPolicyResponse response;
    grpc::Status status = stub->setPolicy(&context, request, &response);
    /////
    bool isRemove = policy.isdeletepolicy();
    std::string policyType = "\"" + policyTypeEnumToString(policy.type()) + "\"";
    /////
    if (isDevcie) {
        (*json)["device_id"] = id;
    } else {
        (*json)["group_id"] = id;
    }
    //std::cout << "--Gang---1----status.ok() = " << status.ok() << std::endl;
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            //Succeed to set the "GPU Core Temperature" policy.
            //Succeed to remove the "GPU Core Temperature" policy.
            (*json)["is_success"] = true;
            if (isRemove) {
                (*json)["msg"] = "Succeed to remove the " + policyType + " policy.";
                XPUM_LOG_AUDIT("Succeed to remove the %s policy.", policyType.c_str());
            } else {
                (*json)["msg"] = "Succeed to set the " + policyType + " policy.";
                XPUM_LOG_AUDIT("Succeed to set the %s policy.", policyType.c_str());
            }
        } else {
            (*json)["is_success"] = false;
            if (isRemove) {
                (*json)["error"] = "Failed to remove the " + policyType + " policy. Error message: " + response.errormsg();
                XPUM_LOG_AUDIT("Failed to remove the %s policy. Error message: %s", policyType.c_str(), response.errormsg().c_str());
            } else {
                (*json)["error"] = "Failed to set the " + policyType + " policy. Error message: " + response.errormsg();
                XPUM_LOG_AUDIT("Failed to set the %s policy. Error message: %s", policyType.c_str(), response.errormsg().c_str());
            }
        }
    } else {
        (*json)["is_success"] = false;
        if (isRemove) {
            (*json)["error"] = "Failed to remove the " + policyType + " policy. Error message: " + status.error_message();
            XPUM_LOG_AUDIT("Failed to remove the %s policy. Error message: %s", policyType.c_str(), status.error_message().c_str());
        } else {
            (*json)["error"] = "Failed to set the " + policyType + " policy. Error message: " + status.error_message();
            XPUM_LOG_AUDIT("Failed to set the %s policy. Error message: %s", policyType.c_str(), status.error_message().c_str());
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPolicy(bool isDevcie, uint32_t id) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GetPolicyRequest request;
    request.set_id(id);
    request.set_isdevcie(isDevcie);
    GetPolicyResponse response;
    grpc::Status status = stub->getPolicy(&context, request, &response);
    std::vector<nlohmann::json> componentJsonList;
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            for (int i = 0; i < response.policylist_size(); i++) {
                if (!isCliSupportedPolicyType(response.policylist(i).type())) {
                    continue;
                }
                auto component = nlohmann::json();
                component["device_id"] = response.policylist(i).deviceid();
                component["type"] = policyTypeEnumToString(response.policylist(i).type());

                //
                XpumPolicyConditionType ctype = response.policylist(i).condition().type();
                std::string condition = policyConditionTypeEnumToString(ctype);
                if (ctype != XpumPolicyConditionType::POLICY_CONDITION_TYPE_WHEN_INCREASE) {
                    condition += " ";
                    condition += std::to_string(response.policylist(i).condition().threshold());
                }
                component["condition"] = condition;

                //
                XpumPolicyActionType atype = response.policylist(i).action().type();
                std::string action = policyActionTypeEnumToString(atype);
                if (atype == XpumPolicyActionType::POLICY_ACTION_TYPE_THROTTLE_DEVICE) {
                    int min = (int)response.policylist(i).action().throttle_device_frequency_min();
                    int max = (int)response.policylist(i).action().throttle_device_frequency_max();
                    action += " min:";
                    action += std::to_string(min);
                    action += " max:";
                    action += std::to_string(max);
                }
                component["action"] = action;

                //
                componentJsonList.push_back(component);
            }
        } else {
            (*json)["is_success"] = false;
            (*json)["error"] = "Failed to list policies. Error message: " + response.errormsg();
            return json;
        }
    }else{
        (*json)["is_success"] = false;
        (*json)["error"] = "Failed to list policies. Error message: " + status.error_message();
        return json;
    }
    if (isDevcie) {
        (*json)["device_id"] = id;
    } else {
        (*json)["group_id"] = id;
    }
    (*json)["policy_list"] = componentJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceConfig(int deviceId, int tileId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceDataRequest request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    ConfigDeviceData response;
    grpc::Status status = stub->getDeviceConfig(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["device_id"] = deviceId;
            (*json)["power_limit"] = response.powerlimit();
            (*json)["power_vaild_range"] = response.powerscope();
            //(*json)["power_average_window"] = response.interval();
            //(*json)["power_average_window_vaild_range"] = response.intervalscope();

            std::vector<nlohmann::json> tileJsonList;
            for (uint i{0}; i < response.tilecount(); ++i) {
                auto tileJson = nlohmann::json();
                tileJson["tile_id"] = response.tileconfigdata(i).tileid();
                tileJson["min_frequency"] = response.tileconfigdata(i).minfreq();
                tileJson["max_frequency"] = response.tileconfigdata(i).maxfreq();
                tileJson["standby_mode"] = standbyModeEnumToString(response.tileconfigdata(i).standby());
                tileJson["scheduler_mode"] = schedulerModeEnumToString(response.tileconfigdata(i).scheduler());
                tileJson["gpu_frequency_valid_options"] = response.tileconfigdata(i).freqoption();
                tileJson["standby_mode_valid_options"] = response.tileconfigdata(i).standbyoption();
                if (int(response.tileconfigdata(i).mediaperformancefactor()) != -1) {
                    tileJson["media_performance_factor"] = int(response.tileconfigdata(i).mediaperformancefactor());
                }
                if (int(response.tileconfigdata(i).computeperformancefactor()) != -1) {
                    tileJson["compute_performance_factor"] = int(response.tileconfigdata(i).computeperformancefactor());
                }
                tileJson["compute_engine"] = "compute";
                tileJson["media_engine"] = "media";
                tileJson["port_up"] = response.tileconfigdata(i).portenabled();
                tileJson["port_down"] = response.tileconfigdata(i).portdisabled();
                tileJson["beaconing_on"] = response.tileconfigdata(i).portbeaconingon();
                tileJson["beaconing_off"] = response.tileconfigdata(i).portbeaconingoff();
                if (response.tileconfigdata(i).schedulertimeout() > 0) {
                    tileJson["scheduler_watchdog_timeout"] = response.tileconfigdata(i).schedulertimeout();
                }
                if (response.tileconfigdata(i).schedulertimesliceinterval() > 0) {
                    tileJson["scheduler_timeslice_interval"] = response.tileconfigdata(i).schedulertimesliceinterval();
                    tileJson["scheduler_timeslice_yield_timeout"] = response.tileconfigdata(i).schedulertimesliceyieldtimeout();
                }
/*
                if (response.tileconfigdata(i).memoryeccavailable() == true) {
                    tileJson["memory_ecc_available"] = "true";
                } else {
                    tileJson["memory_ecc_available"] = "false";
                }
                
                if (response.tileconfigdata(i).memoryeccconfigurable() == true) {
                    tileJson["memory_ecc_configurable"] = "true";
                } else {
                    tileJson["memory_ecc_configurable"] = "false";
                }
                tileJson["memory_ecc_current_state"] = response.tileconfigdata(i).memoryeccstate();
                tileJson["memory_ecc_pending_state"] = response.tileconfigdata(i).memoryeccpendingstate();
                tileJson["memory_ecc_pending_action"] = response.tileconfigdata(i).memoryeccpendingaction();
*/
                (*json)["memory_ecc_current_state"] = response.tileconfigdata(i).memoryeccstate();
                (*json)["memory_ecc_pending_state"] = response.tileconfigdata(i).memoryeccpendingstate();
                tileJsonList.push_back(tileJson);
            }
            (*json)["tile_config_data"] = tileJsonList;
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceSchedulerMode(int deviceId, int tileId, XpumSchedulerMode mode, int val1, int val2) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceSchdeulerModeRequest request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    request.set_scheduler(mode);
    request.set_val1(val1);
    request.set_val2(val2);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDeviceSchedulerMode(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set scheduler mode %d,%d,%d", mode, val1, val2);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set scheduler mode %d,%s", mode, response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set scheduler mode %d,%s", mode, status.error_message().c_str());
    }
    return json;
}
std::unique_ptr<nlohmann::json> LibCoreStub::setDevicePowerlimit(int deviceId, int tileId, int power, int interval) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDevicePowerLimitRequest request;
    request.set_deviceid(deviceId);
    request.set_tileid(tileId);
    request.set_powerlimit(power * 1000);
    request.set_intervalwindow(interval);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDevicePowerLimit(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set power limit %d,%d", power, interval);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set power limit %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set power limit %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceStandby(int deviceId, int tileId, XpumStandbyMode mode) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceStandbyRequest request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    request.set_standby(mode);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDeviceStandbyMode(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set standby mode %d", mode);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set standby mode %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set standby mode %s", status.error_message().c_str());
    }
    return json;
}
std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceFrequencyRangeRequest request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    request.set_minfreq(minFreq);
    request.set_maxfreq(maxFreq);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDeviceFrequencyRange(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set frequency range %d,%d", minFreq, maxFreq);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set frequency range %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set frequency range %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::resetDevice(int deviceId, bool force) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ResetDeviceRequest request;
    ResetDeviceResponse response;

    request.set_deviceid(deviceId);
    request.set_force(force);
    grpc::Status status = stub->resetDevice(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to reset device with force == %d", force);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to reset device with force == %d, errorMessage: %s", force, response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to reset device with force == %d, %s", force, status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPerformanceFactor(int deviceId, int tileId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceDataRequest request;
    DevicePerformanceFactorResponse response;
    request.set_deviceid(deviceId);
    request.set_istiledata(true);
    request.set_tileid(tileId);

    grpc::Status status = stub->getPerformanceFactor(&context, request, &response);
    if (status.ok()) {
        std::vector<nlohmann::json> pfList;
        for (uint i{0}; i < response.count(); ++i) {
            auto pr = nlohmann::json();
            pr["tile_id"] = response.pf(i).tileid();
            pr["engine"] = response.pf(i).engineset();
            pr["factor"] = response.pf(i).factor();
            pfList.push_back(pr);
        }
        (*json)["performance_factor_list"] = pfList;
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setPerformanceFactor(int deviceId, int tileId, xpum_engine_type_flags_t engine, double factor) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    PerformanceFactor request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    request.set_engineset(engine);
    request.set_factor(factor);

    DevicePerformanceFactorSettingResponse response;
    grpc::Status status = stub->setPerformanceFactor(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set performance factor %d,%f", engine, factor);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set performance factor %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set performance factor %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setFabricPortEnabled(int deviceId, int tileId, uint32_t port, uint32_t enabled) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceFabricPortEnabledRequest request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    request.set_portnumber(port);
    request.set_enabled(enabled);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDeviceFabricPortEnabled(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set fabric port Enabled %d,%d", port, enabled);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set fabric port Enabled %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set performance factor Enabled %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setFabricPortBeaconing(int deviceId, int tileId, uint32_t port, uint32_t beaconing) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceFabricPortBeconingRequest request;
    request.set_deviceid(deviceId);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    request.set_portnumber(port);
    request.set_beaconing(beaconing);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDeviceFabricPortBeaconing(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set fabric port Beaconing %d,%d", port, beaconing);
        } else {
            (*json)["error"] = response.errormsg();
            XPUM_LOG_AUDIT("Fail to set fabric port Beaconing %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set fabric port Beaconing %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setMemoryEccState(int deviceId, bool enabled) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDeviceMemoryEccStateRequest request;
    request.set_deviceid(deviceId);
    request.set_enabled(enabled);
    ConfigDeviceMemoryEccStateResultData response;
    grpc::Status status = stub->setDeviceMemoryEccState(&context, request, &response);
    if (response.available() == true) {
        (*json)["memory_ecc_available"] = "true";
    } else {
        (*json)["memory_ecc_available"] = "false";
    }
    if (response.configurable() == true) {
        (*json)["memory_ecc_configurable"] = "true";
    } else {
        (*json)["memory_ecc_configurable"] = "false";
    }
    (*json)["memory_ecc_current_state"] = response.currentstate();
    (*json)["memory_ecc_pending_state"] = response.pendingstate();
    (*json)["memory_ecc_pending_action"] = response.pendingaction();

    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set memory ECC state: available: %s, configurable: %s, current: %s, pending: %s, action: %s",
                           (*json)["memory_ecc_available"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_configurable"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["memory_ecc_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_pending_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["memory_ecc_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
        } else {
            if (response.errormsg().compare("Error")== 0) {
                (*json)["error"] = response.errormsg() +
                " Failed to set memory Ecc state: available: " + std::string((*json)["memory_ecc_available"]) +
                ", configurable: " + std::string((*json)["memory_ecc_configurable"]) +
                ", current: " + std::string((*json)["memory_ecc_current_state"]) +
                ", pending: " + std::string((*json)["memory_ecc_pending_state"]) +
                ", action: " + std::string((*json)["memory_ecc_pending_action"]);
            } else {
                (*json)["error"] = response.errormsg();
            }
            XPUM_LOG_AUDIT("Failed to set memory ECC state: available: %s, configurable: %s, current: %s, pending: %s, action: %s",
                           (*json)["memory_ecc_available"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_configurable"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["memory_ecc_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_pending_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["memory_ecc_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        XPUM_LOG_AUDIT("Fail to set memory ECC state: %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceProcessState(int deviceId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceId request;
    DeviceProcessStateResponse response;

    request.set_id(deviceId);
    grpc::Status status = stub->getDeviceProcessState(&context, request, &response);
    if (status.ok()) {
        std::vector<nlohmann::json> deviceProcessList;
        for (uint i{0}; i < response.count(); ++i) {
            auto proc = nlohmann::json();
            proc["process_id"] = response.processlist(i).processid();
            //proc["mem_size"] = response.processlist(i).memsize();
            //proc["share_mem_size"] = response.processlist(i).sharedsize();
            //proc["engine"] = response.processlist(i).engine();
            proc["process_name"] = response.processlist(i).processname();
            deviceProcessList.push_back(proc);
        }
        (*json)["device_process_list"] = deviceProcessList;
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceComponentOccupancyRatio(int deviceId, int tileId, int samplingInterval) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceComponentOccupancyRatioRequest request;
    DeviceComponentOccupancyRatioResponse response;

    request.set_deviceid(deviceId);
    request.set_samplinginterval(samplingInterval);
    if (tileId == -1) {
        request.set_istiledata(false);
        request.set_tileid(0);
    } else {
        request.set_istiledata(true);
        request.set_tileid(tileId);
    }
    
    grpc::Status status = stub->getDeviceComponentOccupancyRatio(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> tileJsonList;
            for (uint i{0}; i < response.tilecount(); ++i) {
                auto tileJson = nlohmann::json();
                tileJson["not_in_use"] = response.componentoccupancylist(i).notinuse();
                tileJson["workload"] = response.componentoccupancylist(i).workload();
                tileJson["engine"] = response.componentoccupancylist(i).engine();
                tileJson["in_use"] = response.componentoccupancylist(i).inuse();
                tileJson["active"] = response.componentoccupancylist(i).active();
                tileJson["alu_active"] = response.componentoccupancylist(i).aluactive();
                tileJson["xmx_active"] = response.componentoccupancylist(i).xmxactive();
                tileJson["xmx_only"] = response.componentoccupancylist(i).xmxonly();
                tileJson["xmx_fpu_active"] = response.componentoccupancylist(i).xmxfpuactive();
                tileJson["fpu_without_xmx"] = response.componentoccupancylist(i).fpuwithoutxmx();
                tileJson["fpu_only"] = response.componentoccupancylist(i).fpuonly();
                tileJson["em_fpu_active"] = response.componentoccupancylist(i).emfpuactive();
                tileJson["em_int_only"] = response.componentoccupancylist(i).emintonly();
                tileJson["other"] = response.componentoccupancylist(i).other();
                tileJson["stall"] = response.componentoccupancylist(i).stall();
                tileJson["non_occupancy"] = response.componentoccupancylist(i).nonoccupancy();
                tileJson["stall_alu"] = response.componentoccupancylist(i).stallalu();
                tileJson["stall_barrier"] = response.componentoccupancylist(i).stallbarrier();
                tileJson["stall_dep"] = response.componentoccupancylist(i).stalldep();
                tileJson["stall_other"] = response.componentoccupancylist(i).stallother();
                tileJson["stall_inst_fetch"] = response.componentoccupancylist(i).stallinstfetch();
                tileJson["tile_id"] = response.componentoccupancylist(i).tileid();

                tileJsonList.push_back(tileJson);
            }
            (*json)["device_id"] = std::to_string(deviceId);
            (*json)["tile_json_list"] = tileJsonList;
        } else {
            (*json)["error"] = response.errormsg();
        }
        
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceUtilizationByProcess(
        int deviceId, int utilizationInterval) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceUtilizationByProcessRequest request;
    DeviceUtilizationByProcessResponse response;

    request.set_deviceid(deviceId);
    request.set_utilizationinterval(utilizationInterval);
    grpc::Status status = stub->getDeviceUtilizationByProcess(&context,
        request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            return json;
        }
        std::vector<nlohmann::json> utilByProcessList;
        for (uint i{0}; i < response.count(); ++i) {
            auto util = nlohmann::json();
            util["process_id"] = response.processlist(i).processid();
            util["process_name"] = response.processlist(i).processname();
            util["device_id"] = response.processlist(i).deviceid();
            util["mem_size"] = 
                response.processlist(i).memsize() / 1000;
            util["shared_mem_size"] = 
                response.processlist(i).sharedmemsize() / 1000;
            util["rendering_engine_util"] =
                response.processlist(i).renderingengineutil();
            util["copy_engine_util"] = response.processlist(i).copyengineutil();
            util["media_engine_util"] =
                response.processlist(i).mediaengineutil();
            util["media_enhancement_util"] =
                response.processlist(i).mediaenhancementutil();
            util["compute_engine_util"] =
                response.processlist(i).computeengineutil();
            utilByProcessList.push_back(util);
        }
        (*json)["device_util_by_proc_list"] = utilByProcessList;
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAllDeviceUtilizationByProcess(
        int utilizationInterval) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    UtilizationInterval ui;
    DeviceUtilizationByProcessResponse response;

    ui.set_utilinterval(utilizationInterval);
    grpc::Status status = stub->getAllDeviceUtilizationByProcess(&context,
        ui, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            return json;
        }
        std::vector<nlohmann::json> utilByProcessList;
        for (uint i{0}; i < response.count(); ++i) {
            auto util = nlohmann::json();
            util["process_id"] = response.processlist(i).processid();
            util["process_name"] = response.processlist(i).processname();
            util["device_id"] = response.processlist(i).deviceid();
            util["mem_size"] = 
                response.processlist(i).memsize() / 1000;
            util["shared_mem_size"] = 
                response.processlist(i).sharedmemsize() / 1000;
            util["rendering_engine_util"] =
                response.processlist(i).renderingengineutil();
            util["copy_engine_util"] = response.processlist(i).copyengineutil();
            util["media_engine_util"] =
                response.processlist(i).mediaengineutil();
            util["media_enhancement_util"] =
                response.processlist(i).mediaenhancementutil();
            util["compute_engine_util"] =
                response.processlist(i).computeengineutil();
            utilByProcessList.push_back(util);
        }
        (*json)["device_util_by_proc_list"] = utilByProcessList;
    } else {
        (*json)["error"] = status.error_message();
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
    int count{16};
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

} // end namespace xpum::cli
