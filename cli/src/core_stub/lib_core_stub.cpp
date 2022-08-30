/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.cpp
 */

#include "core_stub.h"

#include <grpc++/grpc++.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "logger.h"
#include "xpum_structs.h"
#include "xpum_api.h"
#include "internal_api.h"
#include "lib_core_stub.h"
#include "exit_code.h"

using namespace xpum;

namespace xpum::cli {

LibCoreStub::LibCoreStub() {
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

std::string LibCoreStub::diagnosticResultEnumToString(xpum_diag_task_result_t result) {
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

std::string LibCoreStub::diagnosticTypeEnumToString(xpum_diag_task_type_t type, bool rawComponentTypeStr) {
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

std::unique_ptr<nlohmann::json> LibCoreStub::runDiagnosticsByGroup(uint32_t groupId, int level, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::string LibCoreStub::healthStatusEnumToString(HealthStatusType status) {
    std::string ret;
    switch (status) {
        case HEALTH_STATUS_UNKNOWN:
            ret = "Unknown";
            break;
        case HEALTH_STATUS_OK:
            ret = "OK";
            break;
        case HEALTH_STATUS_WARNING:
            ret = "Warning";
            break;
        case HEALTH_STATUS_CRITICAL:
            ret = "Critical";
            break;
        default:
            break;
    }
    return ret;
}

std::string LibCoreStub::healthTypeEnumToString(HealthType type) {
    std::string ret;
    switch (type) {
        case HEALTH_CORE_THERMAL:
            ret = "core_temperature";
            break;
        case HEALTH_MEMORY_THERMAL:
            ret = "memory_temperature";
            break;
        case HEALTH_POWER:
            ret = "power";
            break;
        case HEALTH_MEMORY:
            ret = "memory";
            break;
        case HEALTH_FABRIC_PORT:
            ret = "xe_link_port";
            break;
        default:
            break;
    }
    return ret;
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

std::unique_ptr<nlohmann::json> LibCoreStub::getPolicyById(bool isDevice, int id) {
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

std::unique_ptr<nlohmann::json> LibCoreStub::setPolicy(bool isDevcie, int id, XpumPolicyData& policy) {
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

bool LibCoreStub::isCliSupportedPolicyType(XpumPolicyType type) {
    if (type == XpumPolicyType::POLICY_TYPE_GPU_TEMPERATURE || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE) {
        return true;
    }
    return false;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getPolicy(bool isDevcie, int id) {
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
                if (standbyArray[i].mode == XPUM_DEFAULT) {
                    tileJson["standby_mode"] = standbyModeEnumToString(STANDBY_DEFAULT);
                } else {
                    tileJson["standby_mode"] = standbyModeEnumToString(STANDBY_NEVER);
                }
                break;
            }
        }
        for (uint32_t i = 0; i < schedulerCount; i++) {
            if (schedulerArray[i].subdevice_Id == tileId) {
                if (schedulerArray[i].mode == XPUM_TIMEOUT) {
                    tileJson["scheduler_mode"] = schedulerModeEnumToString(SCHEDULER_TIMEOUT);
                    tileJson["scheduler_watchdog_timeout"] = schedulerArray[i].val1;
                } else if (schedulerArray[i].mode == XPUM_TIMESLICE) {
                    tileJson["scheduler_mode"] = schedulerModeEnumToString(SCHEDULER_TIMESLICE);
                    tileJson["scheduler_timeslice_interval"] = schedulerArray[i].val1;
                    tileJson["scheduler_timeslice_yield_timeout"] = schedulerArray[i].val2;
                } else if (schedulerArray[i].mode == XPUM_EXCLUSIVE) {
                    tileJson["scheduler_mode"] = schedulerModeEnumToString(SCHEDULER_EXCLUSIVE);
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

std::string LibCoreStub::schedulerModeEnumToString(XpumSchedulerMode mode) {
    std::string ret = "null"; //"SCHEDULER_MODE_NULL";
    switch (mode) {
        case SCHEDULER_TIMEOUT:
            ret = "timeout"; //"SCHEDULER_MODE_TIMEOUT";
            break;
        case SCHEDULER_TIMESLICE:
            ret = "timeslice"; //"SCHEDULER_MODE_TIMESLICE";
            break;
        case SCHEDULER_EXCLUSIVE:
            ret = "exclusive"; //"SCHEDULER_MODE_EXCLUSIVE";
            break;
        case SCHEDULER_DEBUG:
            ret = "debug"; //"SCHEDULER_MODE_DEBUG";
            break;
        default:
            break;
    }
    return ret;
}
std::string LibCoreStub::standbyModeEnumToString(XpumStandbyMode mode) {
    std::string ret = "null"; //"STANDBY_MODE_NULL";
    switch (mode) {
        case STANDBY_DEFAULT:
            ret = "default"; //"STANDBY_MODE_DEFAULT";
            break;
        case STANDBY_NEVER:
            ret = "never"; //"STANDBY_MODE_NEVER";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceSchedulerMode(int deviceId, int tileId, XpumSchedulerMode mode, int val1, int val2) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_result_t res = XPUM_GENERIC_ERROR;

    if (tileId == -1) {
        (*json)["error"] = "Error";
        (*json)["errno"] = XPUM_CLI_ERROR_TILE_NOT_FOUND;
        goto LOG_ERR;
    }

    if (mode == SCHEDULER_TIMEOUT) {
        xpum_scheduler_timeout_t sch_timeout;
        sch_timeout.subdevice_Id = tileId;
        sch_timeout.watchdog_timeout = val1;
        if (val1 < 5000 || val1 > 100000000) {
            (*json)["error"] = "Invalid scheduler timeout value";
            (*json)["errno"] = XPUM_CLI_ERROR_BAD_ARGUMENT;
            goto LOG_ERR;
        }
        res = xpumSetDeviceSchedulerTimeoutMode(deviceId, sch_timeout);
    } else if (mode == SCHEDULER_TIMESLICE) {
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
    } else if (mode == SCHEDULER_EXCLUSIVE) {
        xpum_scheduler_exclusive_t sch_exclusive;
        sch_exclusive.subdevice_Id = tileId;
        res = xpumSetDeviceSchedulerExclusiveMode(deviceId, sch_exclusive);
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
                (*json)["error"] = "Error";
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

std::unique_ptr<nlohmann::json> LibCoreStub::setDeviceStandby(int deviceId, int tileId, XpumStandbyMode mode) {
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

    if (mode == STANDBY_DEFAULT) {
        standby.mode = XPUM_DEFAULT;
    } else if (mode == STANDBY_NEVER) {
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
                (*json)["error"] = "Error";
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
