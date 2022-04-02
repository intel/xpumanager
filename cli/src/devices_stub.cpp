/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file devices_stub.cpp
 */

#include <nlohmann/json.hpp>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "core_stub.h"

namespace xpum::cli {

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
                deviceJson["device_id"] = deviceInfo.id().id();
                deviceJson["device_type"] = deviceInfo.type().value() == 0 ? "GPU" : "Unknown";
                deviceJson["uuid"] = deviceInfo.uuid();
                deviceJson["device_name"] = deviceInfo.devicename();
                deviceJson["pci_device_id"] = deviceInfo.pciedeviceid();
                deviceJson["pci_bdf_address"] = deviceInfo.pcibdfaddress();
                deviceJson["vendor_name"] = deviceInfo.vendorname();
                deviceJsonList.push_back(deviceJson);
            }
            (*json)["device_list"] = deviceJsonList;
        } else {
            (*json)["error"] = response.errormsg();
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

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        return json;
    }

    for (int i{0}; i < response.properties_size(); ++i) {
        auto &p = response.properties(i);
        std::string name = p.name();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        (*json)[name] = p.value();
    }
    (*json)["device_id"] = deviceId;

    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getAMCFirmwareVersions() {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GetAMCFirmwareVersionsResponse response;

    grpc::Status status = stub->getAMCFirmwareVersions(&context, google::protobuf::Empty(), &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        return json;
    }

    for (auto version : response.versions()) {
        (*json)["amc_fw_version"].push_back(version);
    }

    return json;
}

} // end namespace xpum::cli
