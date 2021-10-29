#include "core_stub.h"

#include <grpc++/grpc++.h>

#include <cassert>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>

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
            (*json)["parent_switch_count"] = response.switchcount();

            std::vector<nlohmann::json> switchJsonList;
            for (int i{0}; i < response.switchinfo_size(); ++i) {
                auto switchJson = nlohmann::json();
                switchJson["device_patch"] = response.switchinfo(i).switchdevicepath();
                //std::string switchIdx = std::string("switch_") + std::to_string(i);
                //(*json)[switchIdx] = response.switchinfo(i).switchdevicepath();
                switchJsonList.push_back(switchJson);
            }
            (*json)["switch_list"] = switchJsonList;
        } else {
            (*json)["error message"] = response.errormsg();
        }
    } else {
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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

        std::vector<nlohmann::json> deviceJsonList;

        for (int i{0}; i < response.devicelist_size(); ++i) {
             auto deviceJson = nlohmann::json();
            deviceJson["device_id"] = response.devicelist(i).id();
            deviceJsonList.push_back(deviceJson);
        }

        (*json)["device_list"] = deviceJsonList;
    } else {
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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

                std::vector<nlohmann::json> deviceJsonList;
                for (uint32_t j{0}; j < response.grouplist(i).count(); ++j) {
                    auto deviceJson = nlohmann::json();
                    deviceJson["device_id"] = response.grouplist(i).devicelist(j).id();
                    deviceJsonList.push_back(deviceJson);
                }

                groupJson["device_list"] = deviceJsonList;

                groupJsonList.push_back(groupJson);
            }

            (*json)["group_list"] = groupJsonList;
        } else {
            (*json)["error message"] = response.errormsg();
        }
    } else {
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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

            std::vector<nlohmann::json> deviceJsonList;

            for (int i{0}; i < response.devicelist_size(); ++i) {
                auto deviceJson = nlohmann::json();
                deviceJson["device_id"] = response.devicelist(i).id();
                deviceJsonList.push_back(deviceJson);
            }

            (*json)["device_list"] = deviceJsonList;
        } else {
            (*json)["error message"] = response.errormsg();
        }
    } else {
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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
            (*json)["group_id"] = response.id();
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<nlohmann::json> deviceJsonList;

            for (int i{0}; i < response.devicelist_size(); ++i) {
                auto deviceJson = nlohmann::json();
                deviceJson["device_id"] = response.devicelist(i).id();
                deviceJsonList.push_back(deviceJson);
            }

            (*json)["device_list"] = deviceJsonList;
        } else {
            (*json)["error message"] = response.errormsg();
        }
    } else {
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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
            (*json)["group_id"] = response.id();
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<nlohmann::json> deviceJsonList;

            for (int i{0}; i < response.devicelist_size(); ++i) {
                auto deviceJson = nlohmann::json();
                deviceJson["device_id"] = response.devicelist(i).id();
                deviceJsonList.push_back(deviceJson);
            }

            (*json)["device_list"] = deviceJsonList;
        } else {
            (*json)["error message"] = response.errormsg();
        }
    } else {
        (*json)["error code"] = status.error_code();
        (*json)["error message"] = response.errormsg();
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
            ret = "Env Variables";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_LIBRARY:
            ret = "Library";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_PERMISSION:
            ret = "Permission";
            break;
        case DiagnosticsComponentInfo_Type_DIAG_SOFTWARE_EXCLUSIVE:
            ret = "Exclusive";
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
            while ((*json)["finished"] == false) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
                json = getDiagnosticsResult(deviceId);
                if ((*json).contains("error")) {
                    return json;
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

std::unique_ptr<nlohmann::json> CoreStub::runDiagnosticsByGroup(int groupId, int level) {
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
            while ((*json)["finished"] == false) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
                json = getDiagnosticsResultByGroup(groupId);
                if ((*json).contains("error")) {
                    return json;
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

std::unique_ptr<nlohmann::json> CoreStub::getDiagnosticsResultByGroup(int groupId) {
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
        case HEALTH_THERMAL:
            ret = "temperature";
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
                auto healthJson = (*getHealth(response.info(i).id().id()));
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

std::unique_ptr<nlohmann::json> CoreStub::getHealth(int deviceId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    HealthDataRequest request;
    request.set_deviceid(deviceId);
    HealthData response;
    (*json)["device_id"] = deviceId;
    grpc::Status status = grpc::Status::OK;
    std::vector<HealthType> types = {HEALTH_THERMAL, HEALTH_POWER, HEALTH_MEMORY, HEALTH_FABRIC_PORT};
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
        if (componentJson.contains("threshold")) {
            (*json)[currentHealthType]["threshold"] = componentJson["threshold"];
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
            if (response.type() == HEALTH_POWER) {
                (*json)["threshold"] = getHealthConfig(deviceId, HEALTH_POWER_LIMIT);
            }
            if (response.type() == HEALTH_THERMAL) {
                (*json)["threshold"] = getHealthConfig(deviceId, HEALTH_THEARMAL_LIMIT);
            }
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

std::unique_ptr<nlohmann::json> CoreStub::getHealthByGroup(int groupId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    (*json)["group_id"] = groupId;
    HealthDataByGroupRequest request;
    request.set_groupid(groupId);
    HealthDataByGroup response;
    std::vector<nlohmann::json> deviceJsonList;
    std::vector<HealthType> types = {HEALTH_THERMAL, HEALTH_POWER, HEALTH_MEMORY, HEALTH_FABRIC_PORT};
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
            if (component.contains("threshold")) {
                deviceJsonList[targetDeviceIndex][currentHealthType]["threshold"] = component["threshold"];
            }
        }
    }
    (*json)["device_count"] = deviceJsonList.size();
    (*json)["device_list"] = deviceJsonList;
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getHealthByGroup(int groupId, HealthType type) {
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
                if (response.healthdata(i).type() == HEALTH_POWER) {
                    component["threshold"] = getHealthConfig(response.healthdata(i).deviceid(), HEALTH_POWER_LIMIT);
                }
                if (response.healthdata(i).type() == HEALTH_THERMAL) {
                    component["threshold"] = getHealthConfig(response.healthdata(i).deviceid(), HEALTH_THEARMAL_LIMIT);
                }
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

std::unique_ptr<nlohmann::json> CoreStub::setHealthConfigByGroup(int groupId, HealthConfigType cfgtype, int threshold) {
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

std::unique_ptr<nlohmann::json> CoreStub::getPolicy(int deviceId) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GetPolicyRequest request;
    request.set_id(deviceId);
    XpumPolicyDataArray response;
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
    (*json)["policy_list"] = componentJsonList;
    return json;
}
} // end namespace xpum::cli
