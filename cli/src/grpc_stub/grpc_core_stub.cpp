/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file grpc_core_stub.cpp
 */

#include "core_stub.h"

#include <grpc++/grpc++.h>
#include <grpc/impl/codegen/grpc_types.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "level_zero/zes_api.h"
#include "core.grpc.pb.h"
#include "core.pb.h"
#include "logger.h"
#include "xpum_structs.h"
#include "grpc_core_stub.h"
#include "exit_code.h"
#include "utility.h"

namespace xpum::cli {

GrpcCoreStub::GrpcCoreStub(bool priv) {
    char* xpum_socket_dir_env = std::getenv("XPUM_SOCKET_DIR");
    std::string unixSockDir{xpum_socket_dir_env != NULL ? xpum_socket_dir_env : "/tmp/"};
    if (unixSockDir.length() == 0 || unixSockDir.back() != '/') {
        unixSockDir += "/";
    }
    std::string unixSockName{unixSockDir + (priv ? "xpum_p.sock" : "xpum_up.sock")};
    std::string serverAddr{"unix://" + unixSockName};
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_ENABLE_HTTP_PROXY, 0);
    this->channel = grpc::CreateCustomChannel(serverAddr, grpc::InsecureChannelCredentials(), args);
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
        case DiagnosticsComponentInfo_Type_DIAG_LIGHT_COMPUTATION:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_LIGHT_COMPUTATION" : "Computation Check");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_LIGHT_CODEC:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_LIGHT_CODEC" : "Media Codec Check");
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
        case DiagnosticsComponentInfo_Type_DIAG_MEMORY_ERROR:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_MEMORY_ERROR" : "Memory Error");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_XE_LINK_THROUGHPUT:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_XE_LINK_THROUGHPUT" : "Xe Link Throughput");
            break;
        case DiagnosticsComponentInfo_Type_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT:
            ret = (rawComponentTypeStr ? "XPUM_DIAG_XE_LINK_ALL_TO_ALL_THROUGHPUT" : "Xe Link all-to-all Throughput");
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::runDiagnostics(int deviceId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DiagnosticsTaskInfo response;
    grpc::Status status;
    if (level > 0) {
        RunDiagnosticsRequest request;
        request.set_deviceid(deviceId);
        request.set_level(level);
        stub->runDiagnostics(&context, request, &response);
    } else if (targetTypes.size() > 0) {
        RunMultipleSpecificDiagnosticsRequest request;
        request.set_deviceid(deviceId);
        for (auto& type : targetTypes)
            request.add_types(type);
        stub->runMultipleSpecificDiagnostics(&context, request, &response);      
    } else {
        (*json)["error"] = "Error";
        (*json)["errno"] = errorNumTranslate(XPUM_GENERIC_ERROR);
        return json;
    }
    bool failed = false;
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
            failed = true;
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        failed = true;
    }
    if (!failed) {
        if (level > 0)
            XPUM_LOG_AUDIT("Succeed to run level-%d diagnostics on device %d", level, deviceId);
        else
            XPUM_LOG_AUDIT("Succeed to run type-%s diagnostics on device %d", toString(targetTypes).c_str(), deviceId);
    } else {
        if (level > 0)
            XPUM_LOG_AUDIT("Failed to run level-%d diagnostics on device %d", level, deviceId);
        else
            XPUM_LOG_AUDIT("Failed to run type-%s diagnostics on device %d", toString(targetTypes).c_str(), deviceId);
    }
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
            if (response.level() >= 1 && response.level() <= 3)
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
                if (response.deviceid() != -1 && response.componentinfo(i).type() == DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE) {
                    auto process_list_json = getDeviceProcessState(response.deviceid());
                    if (process_list_json->contains("device_process_list") && (*process_list_json)["device_process_list"].size() > 1) {
                        std::vector<nlohmann::json> processList;
                        for (auto process : (*process_list_json)["device_process_list"]) {
                            if (process["process_name"] != "")
                                processList.push_back(process);
                        }
                        // For consistency, it is needed to determine the number of processes again and update message
                        // Will refactor the code in the future as xpumGetDiagnosticsExclusiveResult
                        if (processList.size() > 1) {
                            componentJson["process_list"] = processList;
                            componentJson["message"] = "Warning: " + std::to_string(processList.size()) + " processses are using the device.";
                        } else {
                            componentJson["message"] = "Pass to check the software exclusive.";
                        }
                    }
                }
                if (response.deviceid() != -1 && response.componentinfo(i).type() == DiagnosticsComponentInfo_Type_DIAG_MEDIA_CODEC 
                    && response.componentinfo(i).result() == DIAG_RESULT_PASS) {
                    componentJson["media_codec_list"] = (*getDiagnosticsMediaCodecResult(response.deviceid(), rawComponentTypeStr))["media_codec_list"];
                }            
                if (response.deviceid() != -1 && response.componentinfo(i).type() == DiagnosticsComponentInfo_Type_DIAG_XE_LINK_THROUGHPUT
                    && response.componentinfo(i).result() == DIAG_RESULT_FAIL) {
                    auto json = getDiagnosticsXeLinkThroughputResult(response.deviceid(), rawComponentTypeStr);
                    if ((*json)["xe_link_throughput_list_count"] > 0) {
                        componentJson["xe_link_throughput_list"] = (*json)["xe_link_throughput_list"];
                        componentJson["xe_link_throughput_list_count"] = (*json)["xe_link_throughput_list_count"];
                    }
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

std::shared_ptr<nlohmann::json> GrpcCoreStub::getDiagnosticsXeLinkThroughputResult(int deviceId, bool rawFpsStr) {
    auto json = std::shared_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceId request;
    request.set_id(deviceId);
    DiagnosticsXeLinkThroughputInfoArray response;
    grpc::Status status = stub->getDiagnosticsXeLinkThroughputResult(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> xeLinkThroughputJsonList;
            for (int i = 0; i < response.datalist_size(); ++i) {
                auto xeLinkThroughput = nlohmann::json();
                if (rawFpsStr) {
                    xeLinkThroughput["device_id"] = response.datalist(i).deviceid();
                    xeLinkThroughput["src_device_id"] = response.datalist(i).srcdeviceid();
                    xeLinkThroughput["src_tile_id"] = response.datalist(i).srctileid();
                    xeLinkThroughput["src_port_id"] = response.datalist(i).srcportid();
                    xeLinkThroughput["dst_device_id"] = response.datalist(i).dstdeviceid();
                    xeLinkThroughput["dst_tile_id"] = response.datalist(i).dsttileid();
                    xeLinkThroughput["dst_port_id"] = response.datalist(i).dstportid();
                    xeLinkThroughput["current_speed"] = response.datalist(i).currentspeed();
                    xeLinkThroughput["max_speed"] = response.datalist(i).maxspeed();
                    xeLinkThroughput["threshold"] = response.datalist(i).threshold();
                } else {
                    xeLinkThroughput["xe_link_throughput"] = " GPU " + std::to_string(response.datalist(i).srcdeviceid()) + "/" 
                    + std::to_string(response.datalist(i).srctileid()) + " port " + std::to_string(response.datalist(i).srcportid()) 
                    + " to GPU " + std::to_string(response.datalist(i).dstdeviceid()) + "/" 
                    + std::to_string(response.datalist(i).dsttileid()) + " port " + std::to_string(response.datalist(i).dstportid()) 
                    + ": " + roundDouble(response.datalist(i).currentspeed(), 3) + " GBPS. Threshold: " + roundDouble(response.datalist(i).threshold(), 3) + " GBPS.";
                }
                xeLinkThroughputJsonList.push_back(xeLinkThroughput); 
            }
            (*json)["xe_link_throughput_list"] = xeLinkThroughputJsonList;
            (*json)["xe_link_throughput_list_count"] = response.datalist_size();
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::runDiagnosticsByGroup(uint32_t groupId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DiagnosticsGroupTaskInfo response;
    grpc::Status status;
    if (level > 0) {
        RunDiagnosticsByGroupRequest request;
        request.set_groupid(groupId);
        request.set_level(level);
        status = stub->runDiagnosticsByGroup(&context, request, &response);
    } else if (targetTypes.size() > 0) {
        RunMultipleSpecificDiagnosticsByGroupRequest request;
        request.set_groupid(groupId);
        for (auto& type : targetTypes)
            request.add_types(type);
        stub->runMultipleSpecificDiagnosticsByGroup(&context, request, &response);  
    } else {
        (*json)["error"] = "Error";
        (*json)["errno"] = errorNumTranslate(XPUM_GENERIC_ERROR);
        return json;
    }
    bool failed = false;
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
            failed = true;
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        failed = true;
    }
    if (!failed) {
        if (level > 0)
            XPUM_LOG_AUDIT("Succeed to run level-%d diagnostics on group %d", level, groupId);
        else
            XPUM_LOG_AUDIT("Succeed to run type-%s diagnostics on group %d", toString(targetTypes).c_str(), groupId);
    } else {
        if (level > 0)
            XPUM_LOG_AUDIT("Failed to run level-%d diagnostics on group %d", level, groupId);
        else
            XPUM_LOG_AUDIT("Failed to run type-%s diagnostics on group %d", toString(targetTypes).c_str(), groupId); 
    }
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
                if (response.taskinfo(i).level() >= 1 && response.taskinfo(i).level() <= 3)
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
                    if (response.taskinfo(i).componentinfo(j).type() == DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE) {
                        auto process_list_json = getDeviceProcessState(response.taskinfo(i).deviceid());
                        if (process_list_json->contains("device_process_list") && (*process_list_json)["device_process_list"].size() > 1) {
                            std::vector<nlohmann::json> processList;
                            for (auto process : (*process_list_json)["device_process_list"]) {
                                if (process["process_name"] != "")
                                    processList.push_back(process);
                            }
                            // For consistency, it is needed to determine the number of processes again and update message
                            // Will refactor the code in the future as xpumGetDiagnosticsExclusiveResult
                            if (processList.size() > 1) {
                                componentJson["process_list"] = processList;
                                componentJson["message"] = "Warning: " + std::to_string(processList.size()) + " processses are using the device.";
                            } else {
                                componentJson["message"] = "Pass to check the software exclusive.";
                            }
                        }
                    }
                    if (response.taskinfo(i).componentinfo(j).type() == DiagnosticsComponentInfo_Type_DIAG_MEDIA_CODEC 
                        && response.taskinfo(i).componentinfo(j).result() == DIAG_RESULT_PASS) {
                        componentJson["media_codec_list"] = (*getDiagnosticsMediaCodecResult(response.taskinfo(i).deviceid(), rawComponentTypeStr))["media_codec_list"];
                    } 
                    if (response.taskinfo(i).componentinfo(j).type() == DiagnosticsComponentInfo_Type_DIAG_XE_LINK_THROUGHPUT
                        && response.taskinfo(i).componentinfo(j).result() == DIAG_RESULT_FAIL) {
                        auto json = getDiagnosticsXeLinkThroughputResult(response.taskinfo(i).deviceid(), rawComponentTypeStr);
                        if ((*json)["xe_link_throughput_list_count"] > 0) {
                            componentJson["xe_link_throughput_list"] = (*json)["xe_link_throughput_list"];
                            componentJson["xe_link_throughput_list_count"] = (*json)["xe_link_throughput_list_count"];
                        }
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



std::string get_diag_test_result(xpum::xpum_diag_result_type_enum val) {
    switch (val) {
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_NO_ERRORS:
            return std::string("No error");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_ABORT:
            return std::string("Abort");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_FAIL_CANT_REPAIR:
            return std::string("Fail cant repair ");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_REBOOT_FOR_REPAIR:
            return std::string("reboot for repair");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_FORCE_UINT32:
            return std::string("Force uint32");
        default:
            return std::string("Unknown");
    }
}

std::string get_diag_test_result_string(xpum::xpum_diag_result_type_enum val) {
    switch (val) {
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_NO_ERRORS:
            return std::string("Diagnostic completed without finding errors to repair.");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_ABORT:
            return std::string("Diagnostic had problems running tests.");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_FAIL_CANT_REPAIR:
            return std::string("Diagnostic had problems setting up repairs.");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_REBOOT_FOR_REPAIR:
            return std::string("Diagnostics found errors, setup for repair and reboot is required to complete the process.");
        case xpum::xpum_diag_result_type_enum::XPUM_DIAG_RESULT_FORCE_UINT32:
            return std::string("");
        default:
            return std::string("Unknown");
    }
}

std::string get_health_state_string(xpum::xpum_health_status_t val) {
    switch (val) {
        case xpum::xpum_health_status_t::XPUM_HEALTH_STATUS_UNKNOWN:
            return std::string("The memory health cannot be determined.");
        case xpum::xpum_health_status_t::XPUM_HEALTH_STATUS_OK:
            return std::string("All memory channels are healthy.");
        case xpum::xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING:
            return std::string("Excessive correctable errors have been detected on one or more channels. Please run \"config --ppr\" to recover this device memory.");
        case xpum::xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL:
            return std::string("Operating with reduced memory to cover banks with too many uncorrectable errors.\nDevice should be replaced due to excessive uncorrectable errors.");
        default:
            return std::string("The memory health cannot be determined.");
    }
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
            for (uint i=0; i < response.pdcount(); i++) {
                std::string domain = "card";
                if (response.powerdomaindata(i).powerdomain() == static_cast<int32_t>(ZES_POWER_DOMAIN_PACKAGE)) {
                    domain = "package";
                }
                for (uint j=0; j<response.powerdomaindata(i).plcount(); j++) {
                    int32_t level = response.powerdomaindata(i).powerlimitdata(j).powerlevel();
                    switch (level){
                    case ZES_POWER_LEVEL_SUSTAINED:
                        (*json)["pl_"+domain+"_sustain"] = response.powerdomaindata(i).powerlimitdata(j).powerlimit();
                        break;
                    case ZES_POWER_LEVEL_PEAK:
                        (*json)["pl_"+domain+"_peak"] = response.powerdomaindata(i).powerlimitdata(j).powerlimit();
                        break;
                    case ZES_POWER_LEVEL_BURST:
                        (*json)["pl_"+domain+"_burst"] = response.powerdomaindata(i).powerlimitdata(j).powerlimit();
                        break;
                    }
                }
            }
            (*json)["power_valid_range"] = response.powerscope();
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
#ifdef DAEMONLESS
                (*json)["pcie_downgrade_current_state"] = response.tileconfigdata(i).pciedowngradestate();
#endif
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::setDevicePowerlimitExt(int device_id, int tile_id,
                                                       const xpum_power_limit_ext_t& plimit_ext) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDevicePowerLimitExtRequest request;
    request.set_deviceid(device_id);
    request.set_tileid(tile_id);
    request.set_powerlimit(plimit_ext.limit);
    request.set_powertype(plimit_ext.level);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDevicePowerLimitExt(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set power limit %d, power level %d", plimit_ext.limit, plimit_ext.level);
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = response.errorno();
            XPUM_LOG_AUDIT("Fail to set power limit %s", response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set power limit %s", status.error_message().c_str());
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
            (*json)["error"] = "Access denied due to permission level or operation unsupported.";
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::applyPPR(int deviceId, bool force){

    assert(this->stub != nullptr);
    auto healthJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext healthContext;
    HealthDataRequest healthRequest;
    healthRequest.set_deviceid(deviceId);
    healthRequest.set_type(HealthType::HEALTH_MEMORY);
    HealthData healthResponse{};
    if(!force){
        grpc::Status status = stub->getHealth(&healthContext, healthRequest, &healthResponse);
        if (status.ok()) {
            if (healthResponse.errormsg().length() == 0) {
                HealthStatusType type = healthResponse.statustype();

                if(type != HealthStatusType::HEALTH_STATUS_WARNING){
                    (*healthJson)["status"] = "OK";
                    (*healthJson)["memory_health_result"] = healthStatusEnumToString(healthResponse.statustype());
                    (*healthJson)["memory_health_result_string"] = healthResponse.description();
                    return healthJson;
                }
            } else {
                (*healthJson)["error"] = healthResponse.errormsg();
                (*healthJson)["errno"] = errorNumTranslate(healthResponse.errorno());
                return healthJson;
            }
        } else {
            (*healthJson)["error"] = status.error_message();
            (*healthJson)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
            return healthJson;
        }
    }

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ApplyPprRequest request;
    ApplyPprResponse response;

    request.set_deviceid(deviceId);
    request.set_force(force);
    grpc::Status status = stub->applyPPR(&context, request, &response);

    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            (*json)["memory_health_result"] = healthStatusEnumToString((HealthStatusType)response.memorystate());
            (*json)["memory_health_result_string"] = get_health_state_string((xpum::xpum_health_status_t)response.memorystate());
            (*json)["ppr_diag_result"] = get_diag_test_result((xpum_diag_result_type_enum)response.diagresult());
            (*json)["ppr_diag_result_string"] = get_diag_test_result_string((xpum_diag_result_type_enum)response.diagresult());
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Fail to apply PPR to device with force == %d, errorMessage: %s", force, response.errormsg().c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to apply PPR to device with force == %d, %s", force, status.error_message().c_str());
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

#ifdef DAEMONLESS
std::unique_ptr<nlohmann::json> GrpcCoreStub::setPCIeDowngradeState(int deviceId, bool enabled) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDevicePCIeDowngradeStateRequest request;
    request.set_deviceid(deviceId);
    request.set_enabled(enabled);
    ConfigDevicePCIeDowngradeStateResultData response;
    grpc::Status status = stub->setDevicePCIeDowngradeState(&context, request, &response);
    if (response.available() == true) {
        (*json)["pcie_downgrade_available"] = "true";
    } else {
        (*json)["pcie_downgrade_available"] = "false";
    }
    (*json)["pcie_downgrade_current_state"] = response.currentstate();
    (*json)["pcie_downgrade_pending_action"] = response.pendingaction();

    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
            XPUM_LOG_AUDIT("Succeed to set PCIe Downgrade state: available: %s, current: %s, action: %s",
                           (*json)["pcie_downgrade_available"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["pcie_downgrade_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["pcie_downgrade_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
        } else {
            if (response.errormsg().compare("Error")== 0) {
                (*json)["error"] = response.errormsg() +
                " Failed to set PCIe Downgrade state: available: " + std::string((*json)["pcie_downgrade_available"]) +
                ", current: " + std::string((*json)["pcie_downgrade_current_state"]) +
                ", action: " + std::string((*json)["pcie_downgrade_pending_action"]);
                (*json)["errno"] = errorNumTranslate(response.errorno());
            } else {
                (*json)["error"] = response.errormsg();
                (*json)["errno"] = errorNumTranslate(response.errorno());
            }
            XPUM_LOG_AUDIT("Failed to set PCIe Downgrade state: available: %s, current: %s, action: %s",
                           (*json)["pcie_downgrade_available"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["pcie_downgrade_current_state"].get_ptr<nlohmann::json::string_t*>()->c_str(),
                           (*json)["pcie_downgrade_pending_action"].get_ptr<nlohmann::json::string_t*>()->c_str());
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Fail to set PCIe Downgrade state: %s", status.error_message().c_str());
    }
    return json;
}
#endif

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
                if (isXeDevice()) {
                    tileJson["alu2_active"] = response.componentoccupancylist(i).alu2active();
                    tileJson["alu2_only"] = response.componentoccupancylist(i).alu2only();
                    tileJson["alu2_alu0_active"] = response.componentoccupancylist(i).alu2alu0active();
                    tileJson["alu0_without_alu2"] = response.componentoccupancylist(i).alu0withoutalu2();
                    tileJson["alu0_only"] = response.componentoccupancylist(i).alu0only();
                    tileJson["alu1_alu0_active"] = response.componentoccupancylist(i).alu1alu0active();
                    tileJson["alu1_int_only"] = response.componentoccupancylist(i).alu1intonly();
                }else {
                    tileJson["xmx_active"] = response.componentoccupancylist(i).xmxactive();
                    tileJson["xmx_only"] = response.componentoccupancylist(i).xmxonly();
                    tileJson["xmx_fpu_active"] = response.componentoccupancylist(i).xmxfpuactive();
                    tileJson["fpu_without_xmx"] = response.componentoccupancylist(i).fpuwithoutxmx();
                    tileJson["fpu_only"] = response.componentoccupancylist(i).fpuonly();
                    tileJson["em_fpu_active"] = response.componentoccupancylist(i).emfpuactive();
                    tileJson["em_int_only"] = response.componentoccupancylist(i).emintonly();
                }
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
        int deviceId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceId request;
    DeviceProcessStateResponse response;

    request.set_id(deviceId);
    grpc::Status status = stub->getDeviceProcessState(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> deviceProcessList;
            for (uint i{0}; i < response.count(); ++i) {
                auto proc = nlohmann::json();
                proc["process_id"] = response.processlist(i).processid();
                proc["mem_size"] = response.processlist(i).memsize() / 1024;
                proc["shared_mem_size"] = response.processlist(i).sharedsize() / 1024;
                proc["device_id"] = deviceId;
                proc["process_name"] = response.processlist(i).processname();
                deviceProcessList.push_back(proc);
            }
            (*json)["device_util_by_proc_list"] = deviceProcessList;
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

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAllDeviceUtilizationByProcess() {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;

    // Get a list of all devices
    XpumDeviceBasicInfoArray response;
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            for (int i = 0; i < response.info_size(); ++i) {
                auto process_list_json = getDeviceUtilizationByProcess(response.info(i).id().id());
                if (process_list_json->contains("device_util_by_proc_list")) {
                    if (!(*json).contains("device_util_by_proc_list")) {
                        (*json)["device_util_by_proc_list"] = nlohmann::json::array();
                    }
                    (*json)["device_util_by_proc_list"].insert(
                    (*json)["device_util_by_proc_list"].end(),
                    (*process_list_json)["device_util_by_proc_list"].begin(),
                    (*process_list_json)["device_util_by_proc_list"].end()
                    );
                }
            }
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
                componentJson["max_bit_rate"] = response.topoinfo(i).maxbitrate();

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

std::unique_ptr<nlohmann::json> GrpcCoreStub::runStress(int deviceId, uint32_t stressTime) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    RunStressRequest request;
    request.set_deviceid(deviceId);
    request.set_stresstime(stressTime);
    DiagnosticsTaskInfo response;
    grpc::Status status = stub->runStress(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Failed to run stress test on device %d", deviceId);
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to run stress test on device %d", deviceId);
    }
    XPUM_LOG_AUDIT("Succeed to run stress test on device %d", deviceId);

    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::checkStress(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    CheckStressRequest request;
    request.set_deviceid(deviceId);
    CheckStressResponse response;
    grpc::Status status = stub->checkStress(&context, request, &response);
     if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            XPUM_LOG_AUDIT("Failed to run stress test on device %d", deviceId);
        } else {
            std::vector<nlohmann::json> tasks;
            for (int i = 0; i < response.taskinfo_size(); i++) {
                auto taskJson = nlohmann::json();
                taskJson["device_id"] = response.taskinfo(i).deviceid();
                taskJson["finished"] = response.taskinfo(i).finished();
                taskJson["message"] = response.taskinfo(i).message();
                tasks.push_back(taskJson);
            }
            (*json)["task_list"] = tasks;
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        XPUM_LOG_AUDIT("Failed to check stress test on device %d", deviceId);
    }
    return json;
}

std::string errorTypeToStr(PrecheckErrorType type) {
    std::string ret = "Unknown";
    switch (type)
    {
        case GUC_NOT_RUNNING: ret = "GuC Not Running"; break;
        case GUC_ERROR: ret = "GuC Error"; break;
        case GUC_INITIALIZATION_FAILED: ret = "GuC Initialization Failed"; break;
        case IOMMU_CATASTROPHIC_ERROR: ret = "IOMMU Catastrophic Error"; break;
        case LMEM_NOT_INITIALIZED_BY_FIRMWARE: ret = "LMEM Not Initialized By Firmware"; break;
        case PCIE_ERROR: ret = "PCIe Error"; break;
        case DRM_ERROR: ret = "DRM Error"; break;
        case GPU_HANG: ret = "GPU Hang"; break;
        case I915_ERROR: ret = "i915 Error"; break;
        case I915_NOT_LOADED: ret = "i915 Not Loaded"; break;
        case LEVEL_ZERO_INIT_ERROR: ret = "Level Zero Init Error"; break;
        case HUC_DISABLED: ret = "HuC Disabled"; break;
        case HUC_NOT_RUNNING: ret = "HuC Not Running"; break;
        case LEVEL_ZERO_METRICS_INIT_ERROR: ret = "Level Zero Metrics Init Error"; break;
        case MEMORY_ERROR: ret = "Memory Error"; break;
        case GPU_INITIALIZATION_FAILED: ret = "GPU Initialization Failed"; break;
        case MEI_ERROR: ret = "MEI Error"; break;
        case XE_ERROR: ret = "XE Error"; break;
        case XE_NOT_LOADED: ret = "XE Not Loaded"; break;
        default: break;
    }
    return ret;
}

std::string errorCategoryToStr(PrecheckErrorCategory category) {
    std::string ret = "Unknown";
    switch (category)
    {
        case PRECHECK_ERROR_CATEGORY_HARDWARE: ret = "Hardware"; break;
        case PRECHECK_ERROR_CATEGORY_KMD: ret = "Kernel Mode Driver"; break;
        case PRECHECK_ERROR_CATEGORY_UMD: ret = "User Mode Driver"; break;
        default: break;
    }
    return ret;
}

std::string errorSeverityToStr(PrecheckErrorSeverity severity) {
    std::string ret = "Unknown";
    switch (severity)
    {
        case PRECHECK_ERROR_SEVERITY_CRITICAL: ret = "Critical"; break;
        case PRECHECK_ERROR_SEVERITY_HIGH: ret = "High"; break;
        case PRECHECK_ERROR_SEVERITY_MEDIUM: ret = "Medium"; break;
        case PRECHECK_ERROR_SEVERITY_LOW: ret = "Low"; break;
        default: break;
    }
    return ret;
}

std::string componentTypeToStr(PrecheckComponentType component_type) {
    std::string ret = "Unknown";
    switch (component_type)
    {
        case PRECHECK_COMPONENT_TYPE_DRIVER: ret = "Driver"; break;
        case PRECHECK_COMPONENT_TYPE_CPU: ret = "CPU"; break;
        case PRECHECK_COMPONENT_TYPE_GPU: ret = "GPU"; break;
        default: break;
    }
    return ret;
}

std::string componentStatusToStr(PrecheckComponentStatus status) {
    std::string ret = "Unknown";
    switch (status)
    {
        case PRECHECK_COMPONENT_STATUS_PASS: ret = "Pass"; break;
        case PRECHECK_COMPONENT_STATUS_FAIL: ret = "Fail"; break;
        default: break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::precheck(xpum_precheck_options options, bool rawComponentTypeStr) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    PrecheckOptionsRequest request;
    request.set_onlygpu(options.onlyGPU);
    request.set_sincetime(options.sinceTime);
    PrecheckComponentInfoListResponse response;
    grpc::Status status = stub->precheck(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            return json;
        }
        std::vector<nlohmann::json> component_list;
        for (uint i{0}; i < response.count(); ++i) {
        auto component_json = nlohmann::json();
        component_json["type"] = componentTypeToStr(response.componentinfolist(i).componenttype());
        if (rawComponentTypeStr) {
            if (response.componentinfolist(i).componenttype() == PRECHECK_COMPONENT_TYPE_CPU)
                component_json["id"] = response.componentinfolist(i).cpuid();
            else if (response.componentinfolist(i).componenttype() == PRECHECK_COMPONENT_TYPE_GPU)
                component_json["bdf"] = response.componentinfolist(i).bdf();
            component_json["status"] = componentStatusToStr(response.componentinfolist(i).status());
            if (response.componentinfolist(i).time() != "")
                component_json["time"] = response.componentinfolist(i).time();
            if (response.componentinfolist(i).errorid() > 0) {
                component_json["error_id"] = response.componentinfolist(i).errorid();
                component_json["error_type"] = errorTypeToStr(static_cast<PrecheckErrorType>(response.componentinfolist(i).errorid() - 1));
            }   
            if (response.componentinfolist(i).errordetail() != "") {
                component_json["error_severity"] = errorSeverityToStr(response.componentinfolist(i).errorseverity());
                component_json["error_detail"] = response.componentinfolist(i).errordetail();
            }
        } else {
            std::vector<nlohmann::json> component_details;
            if (response.componentinfolist(i).componenttype() == PRECHECK_COMPONENT_TYPE_CPU) {
                auto id = nlohmann::json();
                id["field_value"] = "CPU ID: " + std::to_string(response.componentinfolist(i).cpuid());
                component_details.push_back(id);
            } else if (response.componentinfolist(i).componenttype() == PRECHECK_COMPONENT_TYPE_GPU) {
                auto bdf = nlohmann::json();
                bdf["field_value"] = "BDF: " + response.componentinfolist(i).bdf();
                component_details.push_back(bdf);
            }
            auto status = nlohmann::json();
            status["field_value"] = "Status: " + componentStatusToStr(response.componentinfolist(i).status());
            component_details.push_back(status);
            if (response.componentinfolist(i).time() != "") {
                auto time_json = nlohmann::json();
                time_json["field_value"] = "Time: " + response.componentinfolist(i).time();
                component_details.push_back(time_json);
            }
            if (response.componentinfolist(i).errorid() > 0) {
                auto error_id = nlohmann::json();
                error_id["field_value"] = "Error ID: " + std::to_string(response.componentinfolist(i).errorid());
                component_details.push_back(error_id);
                auto error_type = nlohmann::json();
                error_type["field_value"] = "Error Type: " + errorTypeToStr(static_cast<PrecheckErrorType>(response.componentinfolist(i).errorid() - 1));
                component_details.push_back(error_type);
            }
            if (response.componentinfolist(i).errordetail() != "") {
                auto error_severity = nlohmann::json();
                error_severity["field_value"] = "Error Severity: " + errorSeverityToStr(response.componentinfolist(i).errorseverity());
                component_details.push_back(error_severity);
                auto error_detail = nlohmann::json();
                error_detail["field_value"] = "Error Detail: " + response.componentinfolist(i).errordetail();
                component_details.push_back(error_detail);
            }
            component_json["error_details"] = component_details;
        }
        component_list.push_back(component_json);
        }
        (*json)["component_list"] = component_list;
        (*json)["component_count"] = response.count();
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getPrecheckErrorTypes() {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    PrecheckErrorListResponse response;
    grpc::Status status = stub->getPrecheckErrorList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            return json;
        }
        std::vector<nlohmann::json> error_type_list;
        for (uint i{0}; i < response.count(); ++i) {
            auto error_type = nlohmann::json();
            error_type["error_id"] = response.precheckerrorlist(i).errorid();
            error_type["error_type"] = errorTypeToStr(response.precheckerrorlist(i).errortype());
            error_type["error_category"] = errorCategoryToStr(response.precheckerrorlist(i).errorcategory());
            error_type["error_severity"] = errorSeverityToStr(response.precheckerrorlist(i).errorseverity());
            error_type_list.push_back(error_type);
        }
        (*json)["error_type_list"] = error_type_list;
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::genDebugLog(
        const std::string &fileName) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    FileName request;
    request.set_filename(fileName);
    GenDebugLogResponse response;
    grpc::Status status = stub->genDebugLog(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        } else {
            (*json)["status"] = "OK";
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::doVgpuPrecheck() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    VgpuPrecheckResponse response;
    grpc::Status status = stub->doVgpuPrecheck(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        } else {
            (*json)["vmx_flag"] = response.vmxflag() ? "Pass" : "Fail";
            (*json)["vmx_message"] = response.vmxmessage();
            (*json)["iommu_status"] = response.iommustatus() ? "Pass": "Fail";
            (*json)["iommu_message"] = response.iommumessage();
            (*json)["sriov_status"] = response.sriovstatus() ? "Pass": "Fail";
            (*json)["sriov_message"] = response.sriovmessage();
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::createVf(int deviceId, uint32_t numVfs, uint64_t lmem) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    VgpuCreateVfRequest request;
    request.set_numvfs(numVfs);
    request.set_lmempervf(lmem);
    request.set_deviceid(deviceId);
    VgpuCreateVfResponse response;
    grpc::Status status = stub->createVf(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        } else {
            (*json)["status"] = "OK";
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceFunction(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    VgpuGetDeviceFunctionRequest request;
    VgpuGetDeviceFunctionResponse response;
    request.set_deviceid(deviceId);
    grpc::Status status = stub->getDeviceFunction(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        } else {
            std::vector<nlohmann::json> vfs;
            for (uint32_t i = 0; i < response.count(); i++) {
                auto vfInfo = nlohmann::json();
                vfInfo["bdf_address"] = response.functionlist(i).bdfaddress();
                vfInfo["lmem_size"] = response.functionlist(i).lmemsize();
                vfInfo["function_type"] = deviceFunctionTypeEnumToString(
                    static_cast<xpum_device_function_type_t>(response.functionlist(i).devicefunctiontype())
                );
                vfInfo["device_id"] = response.functionlist(i).deviceid() >= 0
                    ? std::to_string(response.functionlist(i).deviceid())
                    : "";
                vfs.push_back(vfInfo);
            }
            (*json)["vf_list"] = vfs;
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::removeAllVf(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    VgpuRemoveAllVfRequest request;
    request.set_deviceid(deviceId);
    VgpuRemoveAllVfResponse response;
    grpc::Status status = stub->removeAllVf(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() > 0) {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        } else {
            (*json)["status"] = "OK";
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getVfMetrics(int deviceId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GetVfMetricsRequest req;
    GetVfMetricsResponse resp;
    req.set_deviceid(deviceId);
    grpc::Status status = stub->getVfMetrics(&context, req, &resp);
    if (status.ok()) {
        if (resp.errormsg().length() > 0) {
            (*json)["error"] = resp.errormsg();
            (*json)["errno"] = errorNumTranslate(resp.errorno());
        } else {
            (*json)["status"] = "OK";
        }
        std::vector<nlohmann::json> vfs;
        for (auto iter : resp.vflist()) {
            auto vf = nlohmann::json();
            vf["bdf_address"] = iter.bdfaddress();
            for (auto &metric : iter.metriclist()) {
                if (metric.scale() == 0) {
                    continue;
                }
                auto val = double(metric.value()) / metric.scale();
                switch (metric.metrictype()) {
                    case XPUM_STATS_GPU_UTILIZATION:
                        vf["gpu_util"] = val;
                        break;
                    case XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
                        vf["ce_util"] = val;
                        break;
                    case XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
                        vf["me_util"] = val;
                        break;
                    case XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION:
                        vf["coe_util"] = val;
                        break;
                    case XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
                        vf["re_util"] = val;
                        break;
                    case XPUM_STATS_MEMORY_UTILIZATION:
                        vf["mem_util"] = val;
                    default:
                        break;
                }
            }
            vfs.push_back(vf);
        }
        (*json)["vf_list"] = vfs;
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
    }
    return json;
}

} // end namespace xpum::cli
