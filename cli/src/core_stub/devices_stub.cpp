/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file devices_stub.cpp
 */

#include <nlohmann/json.hpp>

#include "xpum_api.h"
#include "lib_core_stub.h"
#include "exit_code.h"

using namespace xpum;

namespace xpum::cli {

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceList() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    int count{XPUM_MAX_NUM_DEVICES};
    xpum_device_basic_info devices[XPUM_MAX_NUM_DEVICES];

    xpum_result_t res = xpumGetDeviceList(devices, &count);
    if (res == XPUM_OK) {
        nlohmann::json deviceJsonList;
        for (int i{0}; i < count; ++i) {
            auto deviceJson = nlohmann::json();
            deviceJson["device_id"] = devices[i].deviceId;
            deviceJson["device_type"] = devices[i].type == 0 ? "GPU" : "Unknown";
            deviceJson["uuid"] = devices[i].uuid;
            deviceJson["device_name"] = devices[i].deviceName;
            deviceJson["pci_device_id"] = devices[i].PCIDeviceId;
            deviceJson["pci_bdf_address"] = devices[i].PCIBDFAddress;
            deviceJson["vendor_name"] = devices[i].VendorName;
            deviceJson["drm_device"] = devices[i].drmDevice;
            deviceJsonList.push_back(deviceJson);
        }
        (*json)["device_list"] = deviceJsonList;
    } else {
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

static std::string scale(std::string value, int scale) {
    int64_t ivalue = std::stol(value);
    double fvalue = ivalue / (double)scale;
    return std::to_string(fvalue);
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceProperties(int deviceId, std::string username, std::string password) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_device_properties_t data;
    auto res = xpumGetDeviceProperties(deviceId, &data);    
    if (res == XPUM_OK) {
        for (int i = 0; i < data.propertyLen; i++) {
            auto& p = data.properties[i];
            std::string name = getXpumDevicePropertyNameString(p.name);
            if (name.compare("MAX_FABRIC_PORT_SPEED") == 0) {
                name = "max_fabric_port_speed";
                (*json)[name] = scale(p.value, 1048576);
            } else {
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                (*json)[name] = p.value;
            }
        }
    } else {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            case XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "Device not found";
                break;                
            default:
                (*json)["error"] = "Error";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }
    (*json)["device_id"] = deviceId;

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getDeviceProperties(const char *bdf, std::string username, std::string password) {
    xpum_device_id_t deviceId = -1;
    // No need to check return value as "-1" covers the failure case
    xpumGetDeviceIdByBDF(bdf, &deviceId);
    return getDeviceProperties(deviceId, username, password);
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAMCFirmwareVersions(std::string username, std::string password) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    int count;
    auto res = xpumGetAMCFirmwareVersions(nullptr, &count, username.c_str(), password.c_str());
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Fail to get AMC firmware version count";
        }
        return json;
    }
    std::vector<xpum_amc_fw_version_t> versions(count);
    res = xpumGetAMCFirmwareVersions(versions.data(), &count, username.c_str(), password.c_str());
    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            default:
                (*json)["error"] = "Fail to get AMC firmware versions";
        }
        return json;
    }
    for (auto version : versions) {
        (*json)["amc_fw_version"].push_back(std::string(version.version));
    }
    return json;
}

} // end namespace xpum::cli
