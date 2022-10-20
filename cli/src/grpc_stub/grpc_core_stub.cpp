/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file grpc_core_stub.cpp
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

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "logger.h"
#include "xpum_structs.h"
#include "grpc_core_stub.h"
#include "exit_code.h"

namespace xpum::cli {

GrpcCoreStub::GrpcCoreStub(bool priv) {
    char* xpum_socket_dir_env = std::getenv("XPUM_SOCKET_DIR");
    std::string unixSockDir{xpum_socket_dir_env != NULL ? xpum_socket_dir_env : "/tmp/"};
    if (unixSockDir.length() == 0 || unixSockDir.back() != '/') {
        unixSockDir += "/";
    }
    std::string unixSockName{unixSockDir + (priv ? "xpum_p.sock" : "xpum_up.sock")};
    std::string serverAddr{"unix://" + unixSockName};
    this->channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    this->stub = XpumCoreService::NewStub(this->channel);
}

bool GrpcCoreStub::isChannelReady() {
    grpc::ClientContext context;
    XpumVersionInfoArray response;
    grpc::Status status = stub->getVersion(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        return true;
    } else {
        return channel->GetState(true) == GRPC_CHANNEL_READY;
    }
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getVersion() {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    const std::string notDetected = "Not Detected";

    (*json)["xpum_version"] = notDetected;
    (*json)["xpum_version_git"] = notDetected;
    (*json)["level_zero_version"] = notDetected;

    grpc::ClientContext context;
    XpumVersionInfoArray response;
    grpc::Status status = stub->getVersion(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            for (int i{0}; i < response.versions_size(); ++i) {
                switch (response.versions(i).version().value()) {
                    case XPUM_VERSION:
                        (*json)["xpum_version"] = response.versions(i).versionstring();
                        break;
                    case XPUM_VERSION_GIT:
                        (*json)["xpum_version_git"] = response.versions(i).versionstring();
                        break;
                    case XPUM_VERSION_LEVEL_ZERO:
                        (*json)["level_zero_version"] = response.versions(i).versionstring();
                        break;
                    default:
                        assert(0);
                }
            }
        }
    }

    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeivceIdByBDF(const char* bdf, int *deviceId){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceBDF request;    
    DeviceId response;
    request.set_bdf(bdf);
    grpc::Status status = stub->getDeviceIdByBDF(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["deviceId"] = response.id();
            *deviceId = response.id();
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }

    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getTopology(int deviceId) {
    assert(this->stub != nullptr);
    DeviceId device_id;

    device_id.set_id(deviceId);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    (*json)["device_id"] = deviceId;

    grpc::ClientContext context;
    XpumTopologyInfo response;
    grpc::Status status = stub->getTopology(&context, device_id, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["affinity_localcpulist"] = response.cpuaffinity().localcpulist();
            (*json)["affinity_localcpus"] = response.cpuaffinity().localcpus();
            (*json)["switch_count"] = response.switchcount();

            std::vector<std::string> switchList;
            for (int i{0}; i < response.switchinfo_size(); ++i) {
                switchList.push_back(response.switchinfo(i).switchdevicepath());
            }

            (*json)["switch_list"] = switchList;

        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }

    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::groupCreate(std::string groupName) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupName name;
    name.set_name(groupName);
    grpc::Status status = stub->groupCreate(&context, name, &response);
    if (status.ok()) {
        if( response.errormsg().length() == 0 ) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }

    } else {
        XPUM_LOG_AUDIT("Fail to create group %s", groupName.c_str());
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::groupDelete(int groupId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupId id;
    id.set_id(groupId);
    grpc::Status status = stub->groupDestory(&context, id, &response);
    if (status.ok() ) {
        if(response.errormsg().length() == 0){
            (*json)["group_id"] = response.id();
            XPUM_LOG_AUDIT("Succeed to delete group %d", groupId);
        } else {
            XPUM_LOG_AUDIT("Fail to delete group %d", groupId);
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to delete group %d", groupId);
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::groupListAll() {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupArray response;

    grpc::Status status = stub->getAllGroups(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> groupJsonList;
            for (int i{0}; i < response.grouplist_size(); ++i) {
                auto groupJson = nlohmann::json();
                groupJson["group_id"] = response.grouplist(i).id();
                groupJson["group_name"] = response.grouplist(i).groupname();
                groupJson["device_count"] = response.grouplist(i).count();

                std::vector<int32_t> deviceIdList;
                for (uint32_t j{0}; j < response.grouplist(i).count(); ++j) {
                    deviceIdList.push_back(response.grouplist(i).devicelist(j).id());
                }

                groupJson["device_id_list"] = deviceIdList;

                groupJsonList.push_back(groupJson);
            }

            (*json)["group_list"] = groupJsonList;
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::groupList(int groupId) {
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
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::groupAddDevice(int groupId, int deviceId) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        XPUM_LOG_AUDIT("Fail to add device(%d) to group %d", deviceId, groupId);
        (*json)["device_id"] = deviceId;
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::groupRemoveDevice(int groupId, int deviceId) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        XPUM_LOG_AUDIT("Fail to remove device(%d) from group %d", deviceId, groupId);
        (*json)["device_id"] = deviceId;
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

static std::string diagnosticResultEnumToString(DiagnosticsTaskResult result) {
    std::string ret;
    switch (result) {
        case DIAG_RESULT_UNKNOWN:
            ret = "Unknown";
            break;
        case DIAG_RESULT_PASS:
            ret = "Pass";
            break;
        case DIAG_RESULT_FAIL:
            ret = "Fail";
            break;
        default:
            break;
    }
    return ret;
}

static std::string diagnosticTypeEnumToString(DiagnosticsComponentInfo_Type type, bool rawComponentTypeStr) {
    std::string ret;
    switch (type) {
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_ENV_VARIABLES:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_ENV_VARIABLES" : "Software Env Variables");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_LIBRARY:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_LIBRARY" : "Software Library");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_PERMISSION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_PERMISSION" : "Software Permission");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_SOFTWARE_EXCLUSIVE" : "Software Exclusive");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_HARDWARE_SYSMAN:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_HARDWARE_SYSMAN" : "Hardware Sysman");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_INTEGRATION_PCIE:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_INTEGRATION_PCIE" : "Integration PCIe");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_MEDIA_CODEC:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_MEDIA_CODEC" : "Media Codec");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_COMPUTATION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_COMPUTATION" : "Performance Computation");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_POWER:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_POWER" : "Performance Power");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_MEMORY_ALLOCATION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION" : "Performance Memory Allocation");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_MEMORY_BANDWIDTH:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH" : "Performance Memory Bandwidth");
            break;
        default:
            break;
    }
    return ret;
}

static std::string diagnosticsMediaCodecResolutionEnumToString(DiagnosticsMediaCodecResolution resolution) {
    std::string ret;
    switch (resolution) {
        case DIAG_MEDIA_1080p:
            ret = "1080p";
            break;
        case DIAG_MEDIA_4K:
            ret = "4K";
            break;
        default:
            break;
    }
    return ret;
}

static std::string diagnosticsMediaCodecFormatEnumToString(DiagnosticsMediaCodecFormat format) {
    std::string ret;
    switch (format) {
        case DIAG_MEDIA_H265:
            ret = "H.265";
            break;
        case DIAG_MEDIA_H264:
            ret = "H.264";
            break;
        case DIAG_MEDIA_AV1:
            ret = "AV1";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::runDiagnostics(int deviceId, int level, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    RunDiagnosticsRequest request;
    request.set_deviceid(deviceId);
    request.set_level(level);
    DiagnosticsTaskInfo response;
    grpc::Status status = stub->runDiagnostics(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
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
                    return errorJson;
                }
            }
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Failed to run level-%d diagnostics on device %d", level, deviceId);
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to run level-%d diagnostics on device %d", level, deviceId);
    }
    XPUM_LOG_AUDIT("Succeed to run level-%d diagnostics on device %d", level, deviceId);
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDiagnosticsResult(int deviceId, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceId request;
    request.set_id(deviceId);
    DiagnosticsTaskInfo response;
    grpc::Status status = stub->getDiagnosticsResult(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["device_id"] = response.deviceid();
            (*json)["level"] = response.level();
            (*json)["component_count"] = response.count();
            (*json)["finished"] = response.finished();
            (*json)["result"] = diagnosticResultEnumToString(response.result());
            (*json)["message"] = response.message();
            (*json)["start_time"] = isotimestamp(response.starttime());
            if (response.finished()) {
                (*json)["end_time"] = isotimestamp(response.endtime());
            }
            std::vector<nlohmann::json> componentJsonList;
            for (int i = 0; i < response.componentinfo_size(); ++i) {
                auto componentJson = nlohmann::json();
                componentJson["component_type"] = diagnosticTypeEnumToString(response.componentinfo(i).type(), rawComponentTypeStr);
                componentJson["finished"] = response.componentinfo(i).finished();
                componentJson["message"] = response.componentinfo(i).message();
                componentJson["result"] = diagnosticResultEnumToString(response.componentinfo(i).result());
                if (response.componentinfo(i).type() == DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE && response.componentinfo(i).result() == DIAG_RESULT_FAIL) {
                    auto process_list_json = getDeviceProcessState(response.deviceid());
                    if (process_list_json->contains("device_process_list")) {
                        std::vector<nlohmann::json> processList;
                        for (auto process : (*process_list_json)["device_process_list"]) {
                            if (process["process_name"] != "")
                                processList.push_back(process);
                        }
                        componentJson["process_list"] = processList;
                    }
                }
                if (response.componentinfo(i).type() == DiagnosticsComponentInfo_Type_DIAG_MEDIA_CODEC 
                    && response.componentinfo(i).result() == DIAG_RESULT_PASS) {
                    componentJson["media_codec_list"] = (*getDiagnosticsMediaCodecResult(response.deviceid(), rawComponentTypeStr))["media_codec_list"];
                }                
                componentJsonList.push_back(componentJson);
            }
            (*json)["component_list"] = componentJsonList;
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::shared_ptr<nlohmann::json> GrpcCoreStub::getDiagnosticsMediaCodecResult(int deviceId, bool rawFpsStr) {
    auto json = std::shared_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceId request;
    request.set_id(deviceId);
    DiagnosticsMediaCodecInfoArray response;
    grpc::Status status = stub->getDiagnosticsMediaCodecResult(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> mediaPerfJsonList;
            for (int i = 0; i < response.datalist_size(); ++i) {
                auto perfJson = nlohmann::json();
                std::string resolution = diagnosticsMediaCodecResolutionEnumToString(response.datalist(i).resolution());
                std::string format = diagnosticsMediaCodecFormatEnumToString(response.datalist(i).format());
                if (rawFpsStr) {
                    perfJson[resolution + " " + format] = response.datalist(i).fps();
                } else {
                    perfJson["fps"] = " " + resolution + " " + format + " : " + response.datalist(i).fps();
                }
                
                mediaPerfJsonList.push_back(perfJson); 
            }
            (*json)["media_codec_list"] = mediaPerfJsonList;
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::runDiagnosticsByGroup(uint32_t groupId, int level, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    RunDiagnosticsByGroupRequest request;
    request.set_groupid(groupId);
    request.set_level(level);
    DiagnosticsGroupTaskInfo response;
    grpc::Status status = stub->runDiagnosticsByGroup(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            json = getDiagnosticsResultByGroup(groupId, rawComponentTypeStr);
            if ((*json).contains("error")) {
                return json;
            }

            auto startTime = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
            while ((*json)["finished"] == false) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
                json = getDiagnosticsResultByGroup(groupId, rawComponentTypeStr);
                if ((*json).contains("error")) {
                    return json;
                }

                auto endTime = std::chrono::duration_cast<std::chrono::seconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
                if (endTime - startTime >= 30 * 60) {
                    auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
                    (*errorJson)["error"] = "time out for unknown reasons";
                    return errorJson;
                }
            }
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Failed to run level-%d diagnostics on group %d", level, groupId);
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to run level-%d diagnostics on group %d", level, groupId);
    }
    XPUM_LOG_AUDIT("Succeed to run level-%d diagnostics on group %d", level, groupId);
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupId request;
    request.set_id(groupId);
    DiagnosticsGroupTaskInfo response;
    grpc::Status status = stub->getDiagnosticsResultByGroup(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            bool finished = true;
            (*json)["group_id"] = response.groupid();
            (*json)["device_count"] = response.count();
            std::vector<nlohmann::json> deviceInfoJsonList;
            for (int i = 0; i < response.taskinfo_size(); i++) {
                auto deviceInfoJson = nlohmann::json();
                deviceInfoJson["device_id"] = response.taskinfo(i).deviceid();
                deviceInfoJson["level"] = response.taskinfo(i).level();
                deviceInfoJson["component_count"] = response.taskinfo(i).count();
                deviceInfoJson["finished"] = response.taskinfo(i).finished();
                finished = finished & response.taskinfo(i).finished();
                deviceInfoJson["result"] = diagnosticResultEnumToString(response.taskinfo(i).result());
                deviceInfoJson["message"] = response.taskinfo(i).message();
                deviceInfoJson["start_time"] = isotimestamp(response.taskinfo(i).starttime());
                if (response.taskinfo(i).finished()) {
                    deviceInfoJson["end_time"] = isotimestamp(response.taskinfo(i).endtime());
                }
                std::vector<nlohmann::json> componentJsonList;
                for (int j = 0; j < response.taskinfo(i).componentinfo_size(); j++) {
                    auto componentJson = nlohmann::json();
                    componentJson["component_type"] = diagnosticTypeEnumToString(response.taskinfo(i).componentinfo(j).type(), rawComponentTypeStr);
                    componentJson["finished"] = response.taskinfo(i).componentinfo(j).finished();
                    componentJson["message"] = response.taskinfo(i).componentinfo(j).message();
                    componentJson["result"] = diagnosticResultEnumToString(response.taskinfo(i).componentinfo(j).result());
                    if (response.taskinfo(i).componentinfo(j).type() == DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE && response.taskinfo(i).componentinfo(j).result() == DIAG_RESULT_FAIL) {
                        auto process_list_json = getDeviceProcessState(response.taskinfo(i).deviceid());
                        if (process_list_json->contains("device_process_list")) {
                            std::vector<nlohmann::json> processList;
                            for (auto process : (*process_list_json)["device_process_list"]) {
                                if (process["process_name"] != "")
                                    processList.push_back(process);
                            }
                            componentJson["process_list"] = processList;
                        }
                    }
                    if (response.taskinfo(i).componentinfo(j).type() == DiagnosticsComponentInfo_Type_DIAG_MEDIA_CODEC 
                        && response.taskinfo(i).componentinfo(j).result() == DIAG_RESULT_PASS) {
                        componentJson["media_codec_list"] = (*getDiagnosticsMediaCodecResult(response.taskinfo(i).deviceid(), rawComponentTypeStr))["media_codec_list"];
                    } 
                    componentJsonList.push_back(componentJson);
                }
                deviceInfoJson["component_list"] = componentJsonList;
                deviceInfoJsonList.push_back(deviceInfoJson);
            }
            (*json)["finished"] = finished;
            (*json)["device_list"] = deviceInfoJsonList;
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

nlohmann::json GrpcCoreStub::appendHealthThreshold(int deviceId, nlohmann::json json, HealthType type,
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

static std::string healthStatusEnumToString(HealthStatusType status) {
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

static std::string healthTypeEnumToString(HealthType type) {
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
        case HEALTH_FREQUENCY:
            ret = "frequency";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllHealth() {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getHealth(int deviceId, int componentType) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthDataRequest request;
    request.set_deviceid(deviceId);
    HealthData response;
    (*json)["device_id"] = deviceId;
    grpc::Status status = grpc::Status::OK;
    std::vector<HealthType> types = {HEALTH_CORE_THERMAL, HEALTH_MEMORY_THERMAL, HEALTH_POWER, HEALTH_MEMORY, HEALTH_FABRIC_PORT, HEALTH_FREQUENCY};
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getHealth(int deviceId, HealthType type) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setHealthConfig(int deviceId, int cfgtype, int threshold) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthConfigRequest request;
    request.set_deviceid(deviceId);
    request.set_configtype((HealthConfigType)cfgtype);
    request.set_threshold(threshold);
    HealthConfigInfo response;
    grpc::Status status = stub->setHealthConfig(&context, request, &response);
    std::string healthTypeStr = healthTypeEnumToString((HealthType)cfgtype);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Failed to set health threshold on device %d type %s threshold %d", deviceId, healthTypeStr.c_str(), threshold);
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to set health threshold on device %d type %s threshold %d", deviceId, healthTypeStr.c_str(), threshold);
    }
    XPUM_LOG_AUDIT("Succeed to set health threshold on device %d type %s threshold %d", deviceId, healthTypeStr.c_str(), threshold);
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getHealthByGroup(uint32_t groupId, int componentType) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    (*json)["group_id"] = groupId;
    HealthDataByGroupRequest request;
    request.set_groupid(groupId);
    HealthDataByGroup response;
    std::vector<nlohmann::json> deviceJsonList;
    std::vector<HealthType> types = {HEALTH_CORE_THERMAL, HEALTH_MEMORY_THERMAL, HEALTH_POWER, HEALTH_MEMORY, HEALTH_FABRIC_PORT, HEALTH_FREQUENCY};
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
            (*errorJson)["errno"] = deviceHealthTypeJsons["errno"];
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getHealthByGroup(uint32_t groupId, HealthType type) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    (*json)[healthTypeEnumToString(type)] = componentJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setHealthConfigByGroup(uint32_t groupId, int cfgtype, int threshold) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthConfigByGroupRequest request;
    request.set_groupid(groupId);
    request.set_configtype((HealthConfigType)cfgtype);
    request.set_threshold(threshold);
    HealthConfigByGroupInfo response;
    grpc::Status status = stub->setHealthConfigByGroup(&context, request, &response);
    std::string healthTypeStr = healthTypeEnumToString((HealthType)cfgtype);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Failed to set health threshold on group %d type %s threshold %d", groupId, healthTypeStr.c_str(), threshold);
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to set health threshold on group %d type %s threshold %d", groupId, healthTypeStr.c_str(), threshold);
    }
    XPUM_LOG_AUDIT("Succeed to set health threshold on group %d type %s threshold %d", groupId, healthTypeStr.c_str(), threshold);
    return json;
}

int GrpcCoreStub::getHealthConfig(int deviceId, HealthConfigType cfgtype) {
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

std::string GrpcCoreStub::policyTypeEnumToString(XpumPolicyType type) {
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

std::string GrpcCoreStub::policyConditionTypeEnumToString(XpumPolicyConditionType type) {
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

std::string GrpcCoreStub::policyActionTypeEnumToString(XpumPolicyActionType type) {
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

bool GrpcCoreStub::isCliSupportedPolicyType(XpumPolicyType type) {
    if (type == XpumPolicyType::POLICY_TYPE_GPU_TEMPERATURE || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE || type == XpumPolicyType::POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE) {
        return true;
    }
    return false;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllPolicy() {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getPolicyById(bool isDevice, uint32_t id) {
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllPolicyType() {
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllPolicyConditionType() {
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllPolicyActionType() {
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::setPolicy(bool isDevcie, uint32_t id, PolicyData& policy) {
    XpumPolicyData policy_;
    policy_.set_type((XpumPolicyType)policy.type);
    policy_.mutable_condition()->set_type((XpumPolicyConditionType)policy.condition.type);
    policy_.mutable_condition()->set_threshold(policy.condition.threshold);
    policy_.mutable_action()->set_throttle_device_frequency_max(policy.action.throttle_device_frequency_max);
    policy_.mutable_action()->set_throttle_device_frequency_min(policy.action.throttle_device_frequency_min);
    policy_.mutable_action()->set_type((XpumPolicyActionType)policy.action.type);
    policy_.set_deviceid(policy.deviceId);
    policy_.set_isdeletepolicy(policy.isDeletePolicy);

    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    SetPolicyRequest request;
    request.set_id(id);
    request.set_isdevcie(isDevcie);
    //request.set_allocated_policy(&policy);
    //Answer* ans = detail->mutable_answer();
    XpumPolicyData* p_policy = request.mutable_policy();
    p_policy->CopyFrom(policy_);
    SetPolicyResponse response;
    grpc::Status status = stub->setPolicy(&context, request, &response);
    /////
    bool isRemove = policy_.isdeletepolicy();
    std::string policyType = "\"" + policyTypeEnumToString(policy_.type()) + "\"";
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
                (*json)["errno"] = errorNumTranslate(response.errorno());
                XPUM_LOG_AUDIT("Failed to remove the %s policy. Error message: %s", policyType.c_str(), response.errormsg().c_str());
            } else {
                (*json)["error"] = "Failed to set the " + policyType + " policy. Error message: " + response.errormsg();
                (*json)["errno"] = errorNumTranslate(response.errorno());
                XPUM_LOG_AUDIT("Failed to set the %s policy. Error message: %s", policyType.c_str(), response.errormsg().c_str());
            }
        }
    } else {
        (*json)["is_success"] = false;
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getPolicy(bool isDevcie, uint32_t id) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            return json;
        }
    }else{
        (*json)["is_success"] = false;
        (*json)["error"] = "Failed to list policies. Error message: " + status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
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

std::string GrpcCoreStub::getCardUUID(const std::string& rawUUID) {
    std::string::size_type start = rawUUID.find_last_of('-');
    if (start != std::string::npos) {
        return rawUUID.substr(start + 1, rawUUID.length());
    } else {
        return rawUUID;
    }
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceConfig(int deviceId, int tileId) {
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
                tileJson["standby_mode"] = standbyModeToString(response.tileconfigdata(i).standby());
                tileJson["scheduler_mode"] = schedulerModeToString(response.tileconfigdata(i).scheduler());
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setDeviceSchedulerMode(int deviceId, int tileId, int mode, int val1, int val2) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    XpumSchedulerMode gmode = static_cast<XpumSchedulerMode>(mode);
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
    request.set_scheduler(gmode);
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set scheduler mode %d,%s", mode, response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set scheduler mode %d,%s", mode, status.error_message().c_str());
    }
    return json;
}
std::unique_ptr<nlohmann::json> GrpcCoreStub::setDevicePowerlimit(int deviceId, int tileId, int power, int interval) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set power limit %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set power limit %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setDeviceStandby(int deviceId, int tileId, int mode) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    XpumStandbyMode gmode = static_cast<XpumStandbyMode>(mode);
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
    request.set_standby(gmode);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDeviceStandbyMode(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set standby mode %d", mode);
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set standby mode %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set standby mode %s", status.error_message().c_str());
    }
    return json;
}
std::unique_ptr<nlohmann::json> GrpcCoreStub::setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set frequency range %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set frequency range %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::resetDevice(int deviceId, bool force) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to reset device with force == %d, errorMessage: %s", force, response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to reset device with force == %d, %s", force, status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getPerformanceFactor(int deviceId, int tileId) {
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
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setPerformanceFactor(int deviceId, int tileId, xpum_engine_type_flags_t engine, double factor) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set performance factor %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set performance factor %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setFabricPortEnabled(int deviceId, int tileId, uint32_t port, uint32_t enabled) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set fabric port Enabled %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set performance factor Enabled %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setFabricPortBeaconing(int deviceId, int tileId, uint32_t port, uint32_t beaconing) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to set fabric port Beaconing %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set fabric port Beaconing %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setMemoryEccState(int deviceId, bool enabled) {
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
                (*json)["errno"] = errorNumTranslate(response.errorno());
            } else {
                (*json)["error"] = response.errormsg();
                (*json)["errno"] = errorNumTranslate(response.errorno());
            }
            XPUM_LOG_AUDIT("Failed to set memory ECC state: available: %s, configurable: %s, current: %s, pending: %s, action: %s",
                           (*json)["memory_ecc_available"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_configurable"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["memory_ecc_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(), (*json)["memory_ecc_pending_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["memory_ecc_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set memory ECC state: %s", status.error_message().c_str());
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceProcessState(int deviceId) {
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
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceComponentOccupancyRatio(int deviceId, int tileId, int samplingInterval) {
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
        
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceUtilizationByProcess(
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
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
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllDeviceUtilizationByProcess(
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
            (*json)["errno"] = errorNumTranslate(response.errorno());
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
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}
std::string GrpcCoreStub::getTopoXMLBuffer() {
    assert(this->stub != nullptr);
    std::string result;
    grpc::ClientContext context;
    TopoXMLResponse response;
    grpc::Status status = stub->getTopoXMLBuffer(&context, google::protobuf::Empty(), &response);

    if (status.ok()) {
        result = response.xmlstring();
    }
    return result;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getXelinkTopology() {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumXelinkTopoInfoArray response;
    grpc::Status status = stub->getXelinkTopology(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> topoJsonList;
            for (int i{0}; i < response.topoinfo_size(); ++i) {
                auto componentJson = nlohmann::json();

                componentJson["local_device_id"] = response.topoinfo(i).localdevice().deviceid();
                componentJson["local_on_subdevice"] = response.topoinfo(i).localdevice().onsubdevice();
                componentJson["local_subdevice_id"] = response.topoinfo(i).localdevice().subdeviceid();
                componentJson["local_numa_index"] = response.topoinfo(i).localdevice().numaindex();
                componentJson["local_cpu_affinity"] = response.topoinfo(i).localdevice().cpuaffinity();

                componentJson["remote_device_id"] = response.topoinfo(i).remotedevice().deviceid();
                componentJson["remote_subdevice_id"] = response.topoinfo(i).remotedevice().subdeviceid();

                componentJson["link_type"] = response.topoinfo(i).linktype();

                int nCount = response.topoinfo(i).linkportlist_size();
                if (nCount > 0) {
                    std::vector<uint32_t> portList;

                    for (int n{0}; n < nCount; n++) {
                        uint32_t value = response.topoinfo(i).linkportlist(n);
                        portList.push_back(value);
                    }
                    componentJson["port_list"] = portList;
                }

                topoJsonList.push_back(componentJson);
            }

            (*json)["topo_list"] = topoJsonList;

        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }

    return json;
}

} // end namespace xpum::cli
