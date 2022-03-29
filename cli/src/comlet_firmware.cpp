/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.cpp
 */

#include "comlet_firmware.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>

#include "core_stub.h"

namespace xpum::cli {
ComletFirmware::ComletFirmware() : ComletBase("updatefw", "Update GPU firmware") {
    this->printHelpWhenNoArgs = true;
}

ComletFirmware::~ComletFirmware() {
}

static bool isNumber(const std::string &str) {
    return str.find_first_not_of("0123456789") == std::string::npos;
}

void ComletFirmware::setupOptions() {
    opts = std::unique_ptr<FlashFirmwareOptions>(new FlashFirmwareOptions());

    auto deviceIdOpt = addOption("-d, --device", opts->deviceId, "device ID, optional");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be integer larger than or equal to 0";
        if (!isNumber(str))
            return errStr;
        int value;
        try {
            value = std::stoi(str);
        } catch (const std::out_of_range &oor) {
            return errStr;
        }
        if (value < 0)
            return errStr;
        return std::string();
    });

    auto fwTypeOpt = addOption("-t, --type", opts->firmwareType, "The firmware name. Valid options: GSC, AMC. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.");
    fwTypeOpt->required();
    fwTypeOpt->check([](const std::string &str) {
        std::string errStr = "Invalid firmware type";
        if (str.compare("GSC") == 0 || str.compare("AMC") == 0) {
            return std::string();
        } else {
            return errStr;
        }
    });

    auto fwPathOpt = addOption("-f, --file", opts->firmwarePath, "The firmware image file path on this server");
    fwPathOpt->required();
    fwPathOpt->transform([](const std::string &str) {
        if (FILE *file = fopen(str.c_str(), "r")) {
            fclose(file);
            // get full path of firmware image path
            char resolved_path[PATH_MAX];
            char *fullpath = realpath(str.c_str(), resolved_path);
            return std::string(fullpath);
        } else {
            throw CLI::ValidationError("Invalid file path.");
        }
    });

    opts->deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
}

nlohmann::json ComletFirmware::validateArguments() {
    nlohmann::json result;
    // GSC
    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GSC") == 0) {
        result["error"] = "Updating GSC firmware on all devices is not supported";
        return result;
    }

    // AMC
    if (opts->deviceId != XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("AMC") == 0) {
        result["error"] = "Updating AMC firmware on single device is not supported";
        return result;
    }
    return result;
}

std::unique_ptr<nlohmann::json> ComletFirmware::run() {
    std::unique_ptr<nlohmann::json> json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    // json = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);
    return json;
}

static void printJson(std::shared_ptr<nlohmann::json> json, std::ostream &out, bool raw) {
    if (raw) {
        out << json->dump() << std::endl;
        return;
    } else {
        out << json->dump(4) << std::endl;
        return;
    }
}

static int getIntFirmwareType(std::string firmwareType) {
    if (firmwareType.compare("GSC") == 0)
        return 0;
    if (firmwareType.compare("AMC") == 0)
        return 1;
    return -1;
}

void ComletFirmware::getJsonResult(std::ostream &out, bool raw) {
    auto validateResultJson = validateArguments();
    if (validateResultJson.contains("error")) {
        printJson(std::make_shared<nlohmann::json>(validateResultJson), out, raw);
        return;
    }

    int type = getIntFirmwareType(opts->firmwareType);
    auto uniqueJson = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);
    std::shared_ptr<nlohmann::json> json = std::move(uniqueJson);
    if (json->contains("error")) {
        printJson(json, out, raw);
        return;
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        json = coreStub->getFirmwareFlashResult(opts->deviceId, type);
        if (json->contains("error")) {
            printJson(json, out, raw);
            return;
        }
        if (!json->contains("result")) {
            nlohmann::json tmp;
            tmp["error"] = "Failed to get firmware reuslt";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        }

        std::string flashStatus = (*json)["result"].get<std::string>();

        if (flashStatus.compare("OK") == 0) {
            nlohmann::json tmp;
            tmp["result"] = "OK";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        } else if (flashStatus.compare("FAILED") == 0) {
            nlohmann::json tmp;
            tmp["result"] = "FAILED";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        } else {
            // do nothing
        }
    }
}

void ComletFirmware::getTableResult(std::ostream &out) {
    auto validateResultJson = validateArguments();
    if (validateResultJson.contains("error")) {
        out << "Error: " << validateResultJson["error"].get<std::string>() << std::endl;
        return;
    }

    // warn user
    int type = getIntFirmwareType(opts->firmwareType);
    if (type == 1) { // AMC caution
        std::cout << "CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model. Updating AMC firmware may cause OS to reboot." << std::endl;
        std::cout << "Please comfirm to proceed ( Y/N ) ?" << std::endl;
        std::string confirm;
        std::cin >> confirm;
        if (confirm != "Y" && confirm != "y") {
            out << "update aborted" << std::endl;
            return;
        }
    } else { // GSC caution
        auto allGroups = coreStub->groupListAll();
        if (allGroups != nullptr && allGroups->contains("group_list")) {
            for (auto groupJson : (*allGroups)["group_list"]) {
                int groupId = groupJson["group_id"].get<int>();
                if (groupId & 0x80000000) {
                    auto deviceIdList = groupJson["device_id_list"];
                    for (auto deviceIdInGroup : deviceIdList) {
                        if (deviceIdInGroup.get<int>() == opts->deviceId) {
                            std::cout << "This GPU card has multiple cores. This operation will update all firmwares. Do you want to continue? (y/n) " << std::endl;
                            std::string confirm;
                            std::cin >> confirm;
                            if (confirm != "Y" && confirm != "y") {
                                out << "update aborted" << std::endl;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    // start run
    auto json = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);

    auto status = (*json)["error"];
    if (!status.is_null()) {
        out << "Error: " << status.get<std::string>() << std::endl;
        return;
    }
    out << "Start to update firmware" << std::endl;
    out << "Firmware Name: " << opts->firmwareType << std::endl;
    out << "Image path: " << opts->firmwarePath << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        out << "." << std::flush;

        json = coreStub->getFirmwareFlashResult(opts->deviceId, type);
        if (json->contains("error")) {
            out << "Error: " << (*json)["error"] << std::endl;
            return;
        }
        if (!json->contains("result")) {
            out << "Error: Failed to get firmware reuslt" << std::endl;
            return;
        }

        std::string flashStatus = (*json)["result"].get<std::string>();

        if (flashStatus.compare("OK") == 0) {
            out << "Update firmware successfully. Please reboot OS to take effect." << std::endl;
            return;
        } else if (flashStatus.compare("FAILED") == 0) {
            out << "Update firmware failed" << std::endl;
            return;
        } else {
            // do nothing
        }
    }

    out << "unknown error" << std::endl;
}
} // namespace xpum::cli