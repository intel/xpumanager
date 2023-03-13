/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_reset.cpp
 */

#include "comlet_reset.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "cli_wrapper.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

void ComletReset::setupOptions() {
    this->opts = std::unique_ptr<ComletResetOptions>(new ComletResetOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });
}

std::unique_ptr<nlohmann::json> ComletReset::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    char confirmed;
    (*json)["return"] = "error";
    int targetId = -1;
    if (isNumber(this->opts->deviceId)) {
        targetId = std::stoi(this->opts->deviceId);
    } else {
        auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
        if (convertResult->contains("error")) {
            return convertResult;
        }
    }
    if (targetId >= 0) {
        json = this->coreStub->getDeviceProcessState(targetId);
        std::cout << "The process(es) below are using this device."
                  << "\n";

        for (auto it = (*json)["device_process_list"].begin(); it != (*json)["device_process_list"].end(); ++it) {
            std::cout << "PID: " << (*it)["process_id"] << " ,";
            std::cout << " Command: " << (*it)["process_name"];
            std::cout << "\n";
        }
        //std::cout << json->dump(4) <<"\n";
        std::cout << "All process(es) above will be forcibly killed if you reset it. Do you want to continue? (Y/N):";
        std::cin >> confirmed;
        if (std::tolower(confirmed) == 'y') {
            json = this->coreStub->resetDevice(targetId, true);
        } else {
            json->clear();
            (*json)["status"] = "CANCEL";
            (*json)["return"] = "Reset is cancelled";
        }
    }

    if ((*json)["status"] == "OK") {
        //json->clear();
        (*json)["return"] = "Succeed to reset the GPU " + this->opts->deviceId;
    }
    return json;
}
} // end namespace xpum::cli
