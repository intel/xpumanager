#include "core_stub.h"

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "xpum_structs.h"

#include <cassert>
#include <grpc++/grpc++.h>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>

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

std::unique_ptr<nlohmann::json> CoreStub::getDeviceList() {

    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumDeviceBasicInfoArray response;
    grpc::Status status = stub->getDeviceList(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            std::vector<nlohmann::json> deviceJsonList;
            for (int i{0}; i < response.info_size(); ++i) {
                auto deviceJson = nlohmann::json();
                auto &deviceInfo = response.info(i);
                deviceJson["deviceId"] = deviceInfo.id().id();
                deviceJson["type"] = deviceInfo.type().value();
                deviceJson["uuid"] = deviceInfo.uuid();
                deviceJson["deviceName"] = deviceInfo.devicename();
                deviceJson["pcieDeviceId"] = deviceInfo.pciedeviceid();
                deviceJson["subDeviceId"] = deviceInfo.subdeviceid();
                deviceJson["pciBdfAddress"] = deviceInfo.pcibdfaddress();
                deviceJson["vendorName"] = deviceInfo.vendorname();
                deviceJsonList.push_back(deviceJson);
            }
            (*json)["deviceList"] = deviceJsonList;
        }
    }

    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getDeviceProperties(int deviceId) {

    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumDeviceProperties response;
    DeviceId grpcDeviceId;
    grpcDeviceId.set_id(deviceId);
    grpc::Status status = stub->getDeviceProperties(&context, grpcDeviceId, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            for (int i{0}; i < response.properties_size(); ++i) {
                auto &p = response.properties(i);
                (*json)[p.name()] = p.value();
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
            (*json)["xpum_affinity_localcpulist"] = response.cpuaffinity().localcpulist();
            (*json)["xpum_affinity_localcpus"] = response.cpuaffinity().localcpus();
            (*json)["xpum_parent_switch_count"] = response.switchcount();

            std::vector<nlohmann::json> switchJsonList;            
            for (int i{0}; i < response.switchinfo_size(); ++i) {
                auto switchJson = nlohmann::json();
                switchJson["xpum_device_patch"] = response.switchinfo(i).switchdevicepath();
                //std::string switchIdx = std::string("xpum_switch_") + std::to_string(i);
                //(*json)[switchIdx] = response.switchinfo(i).switchdevicepath();
                switchJsonList.push_back(switchJson);
            }
            (*json)["xpum_switch_list"] = switchJsonList;
        }
    }

    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupCreate(std::string groupName){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupName name;
    name.set_name(groupName);
    grpc::Status status = stub->groupCreate(&context, name, &response);
    if (status.ok() && response.errormsg().length() == 0) {
        (*json)["group_id"] = response.id();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupDelete(int groupId){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupId id;
    id.set_id(groupId);
    grpc::Status status = stub->groupDestory(&context, id, &response);
    if (status.ok() && response.errormsg().length() == 0) {
        (*json)["group_id"] = response.id();
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupListAll(){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupIdArray response;
    
    grpc::Status status = stub->getAllGroupIds(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if(response.errormsg().length() == 0) {            
            std::vector<nlohmann::json> groupJsonList;
            for (int i{0}; i < response.grouplist_size(); ++i){
                auto groupJson = nlohmann::json();
                groupJson["group_id"] = response.grouplist(i).id();
                groupJsonList.push_back(groupJson);                
            }

            (*json)["group_list"] = groupJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupList(int groupId){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupId id;
    id.set_id(groupId);
    grpc::Status status = stub->groupGetInfo(&context, id, &response);
    if (status.ok()) {
        if(response.errormsg().length() == 0){
            
            (*json)["group_id"] = response.id();
            (*json)["group_name"] = response.groupname();
            (*json)["device_count"] = response.count();

            std::vector<nlohmann::json> deviceJsonList;           

            for (int i{0}; i < response.devicelist_size(); ++i){
                auto deviceJson = nlohmann::json();
                deviceJson["device_id"] = response.devicelist(i).id();
                deviceJsonList.push_back(deviceJson);
            }

            (*json)["device_list"] = deviceJsonList;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupAddDevice(int groupId, int deviceId){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupAddRemoveDevice groupAR;
    groupAR.set_groupid(groupId);
    groupAR.set_deviceid(deviceId);
    grpc::Status status = stub->groupAddDevice(&context, groupAR, &response);
    if (status.ok()) {
        if(response.errormsg().length() == 0){
            (*json)["group_id"] = groupId;
            (*json)["device_id"] = deviceId;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::groupRemoveDevice(int groupId, int deviceId){
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GroupInfo response;
    GroupAddRemoveDevice groupAR;
    groupAR.set_groupid(groupId);
    groupAR.set_deviceid(deviceId);
    grpc::Status status = stub->groupRemoveDevice(&context, groupAR, &response);
    if (status.ok()) {
        if(response.errormsg().length() == 0){
            (*json)["group_id"] = groupId;
            (*json)["device_id"] = deviceId;
        }
    }
    return json;
}