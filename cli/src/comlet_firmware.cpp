/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.cpp
 */

#include "comlet_firmware.h"
#include <nlohmann/json.hpp>
#include "core_stub.h"
#include <thread>
#include <chrono>

namespace xpum::cli {
ComletFirmware::ComletFirmware() : ComletBase("updatefw", "Update GPU firmware") {
    this->printHelpWhenNoArgs = true;
}

ComletFirmware::~ComletFirmware() {
}

void ComletFirmware::setupOptions() {
    opts = std::unique_ptr<FlashFirmwareOptions>( new FlashFirmwareOptions() );
    addOption( "-d, --device", opts->deviceId, "device ID, optional" );
    addOption( "-t, --type", opts->firmwareType, "The firmware name. Valid options: GSC, AMC. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far." );
    addOption( "-f, --file", opts->firmwarePath, "The firmware image file path on this server" );

    opts->deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
}

std::unique_ptr<nlohmann::json> ComletFirmware::run() {
    std::unique_ptr<nlohmann::json> json = std::unique_ptr<nlohmann::json>( new nlohmann::json() );

    if ( opts->deviceId < 0 ) {
        (*json)["error"] = "invalid device id";
        return json;
    }

    if (opts->firmwareType != "GSC" && opts->firmwareType != "AMC") {
        (*json)["error"] = "invalid firmware type";
        return json;
    }

    if (opts->firmwarePath.length() == 0) {
        (*json)["error"] = "invalid file name";
        return json;
    }

    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType == "GSC") {
        (*json)["error"] = "upgrading GSC firmware on all devices is not supported";
        return json;
    }

    if (opts->deviceId != XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType == "AMC") {
        (*json)["error"] = "upgrading AMC firmware on single device is not supported";
        return json;
    }

    int type = opts->firmwareType == "GSC" ? 0 : 1;
    if (type == 1) {
        std::cout << "CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model. Updating AMC firmware may cause OS to reboot." << std::endl;
        std::cout << "Please comfirm to proceed ( Y/N ) ?" << std::endl;
        std::string confirm;
        std::cin >> confirm;
        if (confirm != "Y" && confirm != "y") {
            (*json)["error"] = "update aborted";
            return json;
        }
    } else {
        auto allGroups = coreStub->groupListAll();
        if (allGroups != nullptr && allGroups->contains("group_list")) {
            for (auto groupJson : (*allGroups)["group_list"]) {
                int groupId = groupJson["group_id"].get<int>();
                if (groupId & 0x80000000) {
                    auto deviceIdList = groupJson["device_id_list"];
                    for (auto deviceIdInGroup : deviceIdList) {
                        if (deviceIdInGroup.get<int>() == opts->deviceId) {
                            std::cout << "This GPU card has multiple cores. This operation will update all firmwares. Do you want to continue? (y/n) "<< std::endl;
                            std::string confirm;
                            std::cin >> confirm;
                            if (confirm != "Y" && confirm != "y") {
                                (*json)["error"] = "update aborted";
                                return json;
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "Start to update firmware" << std::endl;
    std::cout << "Firmware Name: " << opts->firmwareType << std::endl;
    std::cout << "Image path: " << opts->firmwarePath << std::endl;

    json = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);
    return json;
}

void ComletFirmware::getTableResult(std::ostream &out) {
    auto json = this->run();
    auto status = (*json)["error"];
    if (!status.is_null()) {
        out << status.get<std::string>() << std::endl;
        return;
    }

    status = (*json)["firmware_flash_result"];
    if (!status.is_null()) {
        if (status.get<std::string>().find("OK") != std::string::npos) {
            auto other_devices = (*json)["other_devices"];
            if (!other_devices.is_null()) {
                out << "CAUTIONS: Please also upgrade GSC firmware on device " << other_devices.get<std::string>() << "if not upgraded" << std::endl;
            }
            out << "Update firmware successfully. Please reboot OS to take effect." << std::endl;
        } else {
            out << "Update firmware failed" << std::endl;
        }
        return;
    }

    out << "unknown error" << std::endl;
}
} // namespace xpum::cli