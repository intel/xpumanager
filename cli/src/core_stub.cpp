#include "core_stub.h"

#include <grpc++/grpc++.h>

#include <cassert>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>
#include <thread>
#include <chrono>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "xpum_structs.h"

namespace xpum::cli {

CoreStub::CoreStub() {
    std::string unixSockName{"/tmp/xpum.sock"};    
    std::string serverAddr{"unix://" + unixSockName};
    auto channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    this->stub = XpumCoreService::NewStub(channel);
}

std::unique_ptr<nlohmann::json> CoreStub::getVersion() {
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

std::unique_ptr<nlohmann::json> CoreStub::getTopology(int deviceId) {
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
        }
    } else {
        (*json)["error"] = response.errormsg();
    }

    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupCreate(std::string groupName) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupName name;
    name.set_name(groupName);
    grpc::Status status = stub->groupCreate(&context, name, &response);
    if (status.ok() && response.errormsg().length() == 0) {
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
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupDelete(int groupId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupId id;
    id.set_id(groupId);
    grpc::Status status = stub->groupDestory(&context, id, &response);
    if (status.ok() && response.errormsg().length() == 0) {
        (*json)["group_id"] = response.id();

    } else {
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupListAll() {
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
        }
    } else {
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupList(int groupId) {
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

std::unique_ptr<nlohmann::json> CoreStub::groupAddDevice(int groupId, int deviceId) {
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
            (*json)["group_id"] = groupId;
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<int32_t> deviceIdList;
            for (uint32_t j{0}; j < response.count(); ++j) {
                deviceIdList.push_back(response.devicelist(j).id());
            }

            (*json)["device_id_list"] = deviceIdList;
        } else {
            (*json)["group_id"] = groupId;
            (*json)["device_id"] = deviceId;
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["group_id"] = groupId;
        (*json)["device_id"] = deviceId;
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupRemoveDevice(int groupId, int deviceId) {
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
            (*json)["group_id"] = groupId;
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<int32_t> deviceIdList;
            for (uint32_t j{0}; j < response.count(); ++j) {
                deviceIdList.push_back(response.devicelist(j).id());
            }

            (*json)["device_id_list"] = deviceIdList;
        } else {
            (*json)["group_id"] = groupId;
            (*json)["device_id"] = deviceId;
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["group_id"] = groupId;
        (*json)["device_id"] = deviceId;
        (*json)["error"] = response.errormsg();
    }
    return json;
}

std::string CoreStub::diagnosticResultEnumToString(DiagnosticsComponentInfo_Result result) {
    std::string ret;
    switch (result) {
        case DiagnosticsComponentInfo_Result_DIAG_RESULT_UNKNOWN:
            ret = "Unknown";
            break;
        case DiagnosticsComponentInfo_Result_DIAG_RESULT_PASS:
            ret = "Pass";
            break;
        case DiagnosticsComponentInfo_Result_DIAG_RESULT_WARNING:
            ret = "Warning";
            break;
        case DiagnosticsComponentInfo_Result_DIAG_RESULT_CRITICAL:
            ret = "Critical";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::diagnosticTypeEnumToString(DiagnosticsComponentInfo_Type type) {
    std::string ret;
    switch (type) {
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_ENV_VARIABLES:
            ret = "Software Env Variables";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_LIBRARY:
            ret = "Software Library";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_PERMISSION:
            ret = "Software Permission";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE:
            ret = "Software Exclusive";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_HARDWARE_SYSMAN:
            ret = "Hardware Sysman";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_INTEGRATION_PCIE:
            ret = "Integration PCIe";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_MEDIA_CODEC:
            ret = "Media Codec";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_COMPUTE:
            ret = "Performance Computation";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_POWER:
            ret = "Performance Power";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_PERFORMANCE_MEMORY:
            ret = "Performance Memory";
            break;
        default:
            break;
    }
    return ret;
}
std::unique_ptr<nlohmann::json> CoreStub::runDiagnostics(int deviceId, int level) {
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
            json = getDiagnosticsResult(deviceId);
            if ((*json).contains("error")) {
                return json;
            }

            auto startTime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            while ((*json)["finished"] == false) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
                json = getDiagnosticsResult(deviceId);
                if ((*json).contains("error")) {
                    return json;
                }
                
                auto endTime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (endTime - startTime >= 20 * 60) {
                    auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
                    (*errorJson)["error"] = "time out for unknown reasons";
                    return errorJson;
                }
            }
        } else {
            (*json)["error"] = response.errormsg();     
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getDiagnosticsResult(int deviceId) {
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
            (*json)["message"] = response.message();
            (*json)["start_time"] = isotimestamp(response.starttime());
            if (response.finished()) {
                (*json)["end_time"] = isotimestamp(response.endtime());    
            }
            std::vector<nlohmann::json> componentJsonList;
            for (int i = 0; i < response.componentinfo_size(); ++i) {
                auto componentJson = nlohmann::json();
                componentJson["component_type"] = diagnosticTypeEnumToString(response.componentinfo(i).type());
                componentJson["finished"] = response.componentinfo(i).finished();
                componentJson["message"] = response.componentinfo(i).message();
                componentJson["result"] = diagnosticResultEnumToString(response.componentinfo(i).result());
                componentJsonList.push_back(componentJson);
            }
            (*json)["component_list"] = componentJsonList;
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::runDiagnosticsByGroup(uint32_t groupId, int level) {
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
            json = getDiagnosticsResultByGroup(groupId);
            if ((*json).contains("error")) {
                return json;
            }

            auto startTime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            while ((*json)["finished"] == false) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
                json = getDiagnosticsResultByGroup(groupId);
                if ((*json).contains("error")) {
                    return json;
                }

                auto endTime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (endTime - startTime >= 20 * 60) {
                    auto errorJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
                    (*errorJson)["error"] = "time out for unknown reasons";
                    return errorJson;
                }
            }
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getDiagnosticsResultByGroup(uint32_t groupId) {
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
                deviceInfoJson["message"] = response.taskinfo(i).message();
                deviceInfoJson["start_time"] = isotimestamp(response.taskinfo(i).starttime());
                if (response.taskinfo(i).finished()) {
                    deviceInfoJson["end_time"] = isotimestamp(response.taskinfo(i).endtime());
                }
                std::vector<nlohmann::json> componentJsonList;
                for (int j = 0; j < response.taskinfo(i).componentinfo_size(); j++) {
                    auto componentJson = nlohmann::json();
                    componentJson["component_type"] = diagnosticTypeEnumToString(response.taskinfo(i).componentinfo(j).type());
                    componentJson["finished"] = response.taskinfo(i).componentinfo(j).finished();
                    componentJson["message"] = response.taskinfo(i).componentinfo(j).message();
                    componentJson["result"] = diagnosticResultEnumToString(response.taskinfo(i).componentinfo(j).result());
                    componentJsonList.push_back(componentJson);
                }
                deviceInfoJson["component_list"] = componentJsonList;
                deviceInfoJsonList.push_back(deviceInfoJson);
            }
            (*json)["finished"] = finished;
            (*json)["device_list"] = deviceInfoJsonList;
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::string CoreStub::healthStatusEnumToString(HealthStatusType status) {
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

std::string CoreStub::healthTypeEnumToString(HealthType type) {
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
            ret = "fabric_port";
            break;
        default:
            break;
    }
    return ret;
}

nlohmann::json CoreStub::appendHealthThreshold(int deviceId, nlohmann::json json, HealthType type) {
    if (type == HEALTH_POWER) {
        json["custom_threshold"] = getHealthConfig(deviceId, HEALTH_POWER_LIMIT);
        json["shutdown_threshold"] = "150 watts";
    }
    if (type == HEALTH_CORE_THERMAL) {
        json["custom_threshold"] = getHealthConfig(deviceId, HEALTH_CORE_THEARMAL_LIMIT);
        json["throttle_threshold"] = "105 Celsius Degree";
        json["shutdown_threshold"] = "130 Celsius Degree";
    }
    if (type == HEALTH_MEMORY_THERMAL) {
        json["custom_threshold"] = getHealthConfig(deviceId, HEALTH_MEMORY_THEARMAL_LIMIT);
        json["throttle_threshold"] = "85 Celsius Degree";
        json["shutdown_threshold"] = "100 Celsius Degree";
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getAllHealth() {
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
            (*json)["all_health_list"] = healthJsonList;
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getHealth(int deviceId, int componentType) {
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

std::unique_ptr<nlohmann::json> CoreStub::getHealth(int deviceId, HealthType type) {
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
            (*json) = appendHealthThreshold(deviceId, (*json), response.type());
        } else {
            (*json)["error"] = response.errormsg();
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::setHealthConfig(int deviceId, HealthConfigType cfgtype, int threshold) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthConfigRequest request;
    request.set_deviceid(deviceId);
    request.set_configtype(cfgtype);
    request.set_threshold(threshold);
    HealthConfigInfo response;
    grpc::Status status = stub->setHealthConfig(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();        
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getHealthByGroup(uint32_t groupId, int componentType) {
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

std::unique_ptr<nlohmann::json> CoreStub::getHealthByGroup(uint32_t groupId, HealthType type) {
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
                component = appendHealthThreshold(response.healthdata(i).deviceid(), component, response.type());

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

std::unique_ptr<nlohmann::json> CoreStub::setHealthConfigByGroup(uint32_t groupId, HealthConfigType cfgtype, int threshold) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthConfigByGroupRequest request;
    request.set_groupid(groupId);
    request.set_configtype(cfgtype);
    request.set_threshold(threshold);
    HealthConfigByGroupInfo response;
    grpc::Status status = stub->setHealthConfigByGroup(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();        
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}

int CoreStub::getHealthConfig(int deviceId, HealthConfigType cfgtype) {
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

std::string CoreStub::isotimestamp(uint64_t t) {
    time_t seconds = (long)t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = gmtime(&seconds);
    // tm *tm_p = localtime(&seconds);
    char buf[50];
    strftime(buf, sizeof(buf), "%FT%T", tm_p);
    char milli_buf[10];
    sprintf(milli_buf, "%03d", milli_seconds);
    return std::string(buf) + "." + std::string(milli_buf) + "Z";
}

std::string CoreStub::policyTypeEnumToString(XpumPolicyType type) {
    std::string ret = "POLICY_TYPE_MAX";
    switch (type) {
        case POLICY_TYPE_GPU_TEMPERATURE:
            ret = "POLICY_TYPE_GPU_TEMPERATURE";
            break;
        case POLICY_TYPE_GPU_MEMORY_TEMPERATURE:
            ret = "POLICY_TYPE_GPU_MEMORY_TEMPERATURE";
            break;
        case POLICY_TYPE_GPU_POWER:
            ret = "POLICY_TYPE_GPU_POWER";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_RESET:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_RESET";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE";
            break;
        case POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
            ret = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::policyConditionTypeEnumToString(XpumPolicyConditionType type) {
    std::string ret = "POLICY_CONDITION_TYPE_GREATER";
    switch (type) {
        case POLICY_CONDITION_TYPE_GREATER:
            ret = "POLICY_CONDITION_TYPE_GREATER";
            break;
        case POLICY_CONDITION_TYPE_LESS:
            ret = "POLICY_CONDITION_TYPE_LESS";
            break;
        case POLICY_CONDITION_TYPE_WHEN_INCREASE:
            ret = "POLICY_CONDITION_TYPE_WHEN_INCREASE";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::policyActionTypeEnumToString(XpumPolicyActionType type) {
    std::string ret = "POLICY_ACTION_TYPE_NULL";
    switch (type) {
        case POLICY_ACTION_TYPE_NULL:
            ret = "POLICY_ACTION_TYPE_NULL";
            break;
        case POLICY_ACTION_TYPE_THROTTLE_DEVICE:
            ret = "POLICY_ACTION_TYPE_THROTTLE_DEVICE";
            break;
        case POLICY_ACTION_TYPE_RESET_DEVICE:
            ret = "POLICY_ACTION_TYPE_RESET_DEVICE";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> CoreStub::getAllPolicy() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> dataList;
            for (int i = 0; i < response.info_size(); i++) {
                auto healthJson = (*getPolicy(true,response.info(i).id().id()));
                dataList.push_back(healthJson);
            }
            (*json)["all_policy_list"] = dataList;
        }
    }
    return json;
}

// std::unique_ptr<nlohmann::json> CoreStub::getAllPolicyType() {
//     assert(this->stub != nullptr);
//     grpc::ClientContext context;
//     XpumDeviceBasicInfoArray response;
//     auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
//     grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
//     if (status.ok()) {
//         if (response.errormsg().length() == 0) {
//             std::vector<nlohmann::json> healthJsonList;
//             healthJsonList.push_back("POLICY_TYPE_GPU_TEMPERATURE");
//             healthJsonList.push_back("POLICY_TYPE_GPU_MEMORY_TEMPERATURE");
//             healthJsonList.push_back("POLICY_TYPE_GPU_POWER");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_RESET");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE");
//             healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE");
//             (*json)["all_policy_type"] = healthJsonList;
//         }
//     }
//     return json;
// }

std::unique_ptr<nlohmann::json> CoreStub::getAllPolicyType() {
    assert(this->stub != nullptr);
    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            // healthJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
            // healthJsonList.push_back("POLICY_CONDITION_TYPE_LESS");
            // healthJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");

            // healthJsonList.push_back("POLICY_ACTION_TYPE_NULL");
            // healthJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
            // healthJsonList.push_back("POLICY_ACTION_TYPE_RESET_DEVICE");

            std::vector<nlohmann::json> healthJsonList;
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_GPU_TEMPERATURE";
                std::vector<nlohmann::json> conditionJsonList;
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_LESS");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_GPU_MEMORY_TEMPERATURE";
                std::vector<nlohmann::json> conditionJsonList;
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_LESS");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                //actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_GPU_POWER";
                std::vector<nlohmann::json> conditionJsonList;
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_LESS");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                //actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_RESET";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                //actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                //actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                //actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                //actionJsonList.push_back("POLICY_ACTION_TYPE_THROTTLE_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                actionJsonList.push_back("POLICY_ACTION_TYPE_RESET_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_GREATER");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                actionJsonList.push_back("POLICY_ACTION_TYPE_RESET_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            {
                auto component = nlohmann::json();
                component["policyType"] = "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE";
                std::vector<nlohmann::json> conditionJsonList;                
                conditionJsonList.push_back("POLICY_CONDITION_TYPE_WHEN_INCREASE");
                component["suppotedConditionType"] = conditionJsonList;
                std::vector<nlohmann::json> actionJsonList;
                actionJsonList.push_back("POLICY_ACTION_TYPE_NULL");
                actionJsonList.push_back("POLICY_ACTION_TYPE_RESET_DEVICE");
                component["suppotedActionType"] = actionJsonList;
                healthJsonList.push_back(component);
            }
            // healthJsonList.push_back("POLICY_TYPE_GPU_TEMPERATURE");
            // healthJsonList.push_back("POLICY_TYPE_GPU_MEMORY_TEMPERATURE");
            // healthJsonList.push_back("POLICY_TYPE_GPU_POWER");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_RESET");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE");
            // healthJsonList.push_back("POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE");
            (*json)["all_policy_type"] = healthJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getAllPolicyConditionType() {
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

std::unique_ptr<nlohmann::json> CoreStub::getAllPolicyActionType() {
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
            healthJsonList.push_back("POLICY_ACTION_TYPE_RESET_DEVICE");
            (*json)["all_policy_list"] = healthJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::setPolicy(bool isDevcie,int id,XpumPolicyData &policy) {
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
    if(isDevcie){
        (*json)["deviceId"] = id;
    }else{
        (*json)["groupId"] = id;
    }   
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "set policy successfully.";
        }else{
            (*json)["status"] = "set policy failed. Error message: "+response.errormsg().length();
        }
    }else{
        (*json)["status"] = "set policy failed. Error message: unknown.";
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getPolicy(bool isDevcie,int id) {
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
                auto component = nlohmann::json();
                component["device_id"] = response.policylist(i).deviceid();
                component["type"] = policyTypeEnumToString(response.policylist(i).type());

                //
                auto condition = nlohmann::json();
                XpumPolicyConditionType ctype = response.policylist(i).condition().type();
                condition["type"] = policyConditionTypeEnumToString(ctype);
                if (ctype != XpumPolicyConditionType::POLICY_CONDITION_TYPE_WHEN_INCREASE) {
                    condition["threshold"] = response.policylist(i).condition().threshold();
                }
                component["condition"] = condition;

                //
                auto action = nlohmann::json();
                XpumPolicyActionType atype = response.policylist(i).action().type();
                action["type"] = policyActionTypeEnumToString(atype);
                if (atype == XpumPolicyActionType::POLICY_ACTION_TYPE_THROTTLE_DEVICE) {
                    action["throttle_device_frequency_max"] = response.policylist(i).action().throttle_device_frequency_max();
                    action["throttle_device_frequency_min"] = response.policylist(i).action().throttle_device_frequency_min();
                }
                component["action"] = action;
                componentJsonList.push_back(component);
            }
        }
    }    
    if(isDevcie){
        (*json)["deviceId"] = id;
    }else{
        (*json)["groupId"] = id;
    }    
    (*json)["policy_list"] = componentJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::runFirmwareFlash( int deviceId, unsigned int type, std::string& filePath ) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;

    XpumFirmwareFlashJob request;
    request.mutable_id()->set_id( deviceId );
    request.mutable_type()->set_value( type );
    request.set_path( filePath );

    GeneralEnum response;
    grpc::Status status = stub->runFirmwareFlash( &context, request, &response );
    if ( status.ok() && response.value() == 0 ) {
        while ( true ) {
            std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
            grpc::ClientContext ct;
            XpumFirmwareFlashTaskRequest rq;
            rq.mutable_id()->set_id( deviceId );
            rq.mutable_type()->set_value( type );

            XpumFirmwareFlashTaskResult res;
            status = stub->getFirmwareFlashResult( &ct, rq, &res );
            if ( status.ok() && res.errormsg().length() == 0 ) {
                if ( res.mutable_result()->value() == 0 ) {
                    (*json)["firmware_flash_result"] = "OK";
                    return json;
                }
                else if ( res.mutable_result()->value() == 1 ) {
                    (*json)["firmware_flash_result"] = "FAILED";
                    return json;
                }
                else {
                    //nothing
                }
            }
            else {
                (*json)["error"] = "Failed to get firmware reuslt";
                return json;
            }
        }
    }
    else {
        (*json)["error"] = "Failed to run firmware flash";
        return json;
    }
}
std::unique_ptr<nlohmann::json> CoreStub::getDeviceConfig(int deviceId, int tileId) {
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
            (*json)["power_average_window"] = response.interval();
            std::vector<nlohmann::json> tileJsonList;
            for (uint i{0}; i < response.tilecount(); ++i) {
                auto tileJson = nlohmann::json();
                tileJson["tile_id"] = response.tileconfigdata(i).tileid();
                tileJson["min_frequency"] = response.tileconfigdata(i).minfreq();
                tileJson["max_frequency"] = response.tileconfigdata(i).maxfreq();
                tileJson["standby_mode"] = standbyModeEnumToString(response.tileconfigdata(i).standby());
                tileJson["scheduler_mode"] = schedulerModeEnumToString(response.tileconfigdata(i).scheduler());
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
std::string CoreStub::schedulerModeEnumToString(XpumSchedulerMode mode) {
    std::string ret = "SCHEDULER_MODE_NULL";
    switch (mode) {
        case SCHEDULER_TIMEOUT:
            ret = "SCHEDULER_MODE_TIMEOUT";
            break;
        case SCHEDULER_TIMESLICE:
            ret = "SCHEDULER_MODE_TIMESLICE";
            break;
        case SCHEDULER_EXCLUSIVE:
            ret = "SCHEDULER_MODE_EXCLUSIVE";
            break;
        case SCHEDULER_DEBUG:
            ret = "SCHEDULER_MODE_DEBUG";
            break;
        default:
            break;
    }
    return ret;
}
std::string CoreStub::standbyModeEnumToString(XpumStandbyMode mode) {
    std::string ret = "STANDBY_MODE_NULL";
    switch (mode) {
        case STANDBY_DEFAULT:
            ret = "STANDBY_MODE_DEFAULT";
            break;
        case STANDBY_NEVER:
            ret = "STANDBY_MODE_NEVER";
            break;
        default:
            break;
    }
    return ret;
}

std::unique_ptr<nlohmann::json> CoreStub::setDeviceSchedulerMode(int deviceId, int tileId, XpumSchedulerMode mode, int val1, int val2){
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
        } else {
            (*json)["error"] = response.errormsg();        
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}
std::unique_ptr<nlohmann::json> CoreStub::setDevicePowerlimit(int deviceId, int power, int interval){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    ConfigDevicePowerLimitRequest request;
    request.set_deviceid(deviceId);
    request.set_powerlimit(power*1000);
    request.set_intervalwindow(interval);

    ConfigDeviceResultData response;
    grpc::Status status = stub->setDevicePowerLimit(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["status"] = "OK";
        } else {
            (*json)["error"] = response.errormsg();        
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}
std::unique_ptr<nlohmann::json> CoreStub::setDeviceStandby(int deviceId, int tileId, XpumStandbyMode mode){
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
        } else {
            (*json)["error"] = response.errormsg();        
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}
std::unique_ptr<nlohmann::json> CoreStub::setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq){
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
        } else {
            (*json)["error"] = response.errormsg();        
        }
    } else {
        (*json)["error"] = status.error_message();
    }
    return json;
}
} // end namespace xpum::cli
