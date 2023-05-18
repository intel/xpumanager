/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file devices_stub.cpp
 */

#include <nlohmann/json.hpp>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "grpc_core_stub.h"
#include "exit_code.h"

namespace xpum::cli {

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceList() {
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
                deviceJson["drm_device"] = deviceInfo.drmdevice();
                deviceJson["device_function_type"] = deviceFunctionTypeEnumToString(
                    static_cast<xpum_device_function_type_t>(deviceInfo.devicefunctiontype())
                );
                deviceJsonList.push_back(deviceJson);
            }
            (*json)["device_list"] = deviceJsonList;
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
        }
    }

    return json;
}

static std::string scale(std::string value, int scale) {
    int64_t ivalue = std::stol(value);
    double fvalue = ivalue / (double)scale;
    return std::to_string(fvalue);
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceProperties(int deviceId, std::string username, std::string password) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    XpumDeviceProperties response;
    DeviceId grpcDeviceId;
    grpcDeviceId.set_id(deviceId);
    grpc::Status status = stub->getDeviceProperties(&context, grpcDeviceId, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    for (int i{0}; i < response.properties_size(); ++i) {
        auto &p = response.properties(i);
        std::string name = p.name();
        if (name.compare("MAX_FABRIC_PORT_SPEED") == 0) {
            name = "max_fabric_port_speed";
            (*json)[name] = scale(p.value(), 1048576);
        } else {
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            (*json)[name] = p.value();
        }
    }
    (*json)["device_id"] = deviceId;

    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getDeviceProperties(const char *bdf, std::string username, std::string password) {
    assert(this->stub != nullptr);
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    DeviceBDF request;    
    DeviceId response;
    request.set_bdf(bdf);
    int deviceId = -1;
    grpc::Status status = stub->getDeviceIdByBDF(&context, request, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            deviceId = response.id();
        } else {
            (*json)["error"] = response.errormsg();
            (*json)["errno"] = errorNumTranslate(response.errorno());
            return json;
        }
    } else {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }
    return getDeviceProperties(deviceId, username, password);
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getSerailNumberAndAmcVersion(int deviceId, std::string username, std::string password) {
    grpc::ClientContext sn_context;
    GetDeviceSerialNumberRequest sn_req;
    GetDeviceSerialNumberResponse sn_res;
    sn_req.set_deviceid(deviceId);
    sn_req.set_username(username);
    sn_req.set_password(password);
    auto status = stub->getDeviceSerialNumberAndAmcFwVersion(&sn_context, sn_req, &sn_res);
    nlohmann::json json;
    json["serial_number"] = "";
    json["amc_firmware_version"] = "";
    if (status.ok()) {
        if (!sn_res.serialnumber().empty())
            json["serial_number"] = sn_res.serialnumber();
        if (!sn_res.amcfwversion().empty())
            json["amc_firmware_version"] = sn_res.amcfwversion();
    }
    return std::make_unique<nlohmann::json>(json);
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAMCFirmwareVersions(std::string username, std::string password) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    grpc::ClientContext context;
    GetAMCFirmwareVersionsRequest request;
    GetAMCFirmwareVersionsResponse response;

    request.set_username(username);
    request.set_password(password);

    grpc::Status status = stub->getAMCFirmwareVersions(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    for (auto version : response.versions()) {
        (*json)["amc_fw_version"].push_back(version);
    }

    return json;
}

} // end namespace xpum::cli
