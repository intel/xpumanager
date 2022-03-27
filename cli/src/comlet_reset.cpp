/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_reset.cpp
 */

#include "comlet_reset.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "cli_wrapper.h"
#include "core_stub.h"

namespace xpum::cli {

void ComletReset::setupOptions() {
    this->opts = std::unique_ptr<ComletResetOptions>(new ComletResetOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
}

std::unique_ptr<nlohmann::json> ComletReset::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    char confirmed;
    (*json)["return"] = "error";
    if (this->opts->deviceId >= 0) {
        json = this->coreStub->getDeviceProcessState(this->opts->deviceId);
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
            json = this->coreStub->resetDevice(this->opts->deviceId, true);
        } else {
            json->clear();
            (*json)["status"] = "CANCEL";
            (*json)["return"] = "Reset is cancelled";
        }
    }

    if ((*json)["status"] == "OK") {
        //json->clear();
        (*json)["return"] = "Succeed to reset the GPU " + std::to_string(this->opts->deviceId);
    }
    return json;
}
} // end namespace xpum::cli
