/* 
 *  Copyright (C) 2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_ps.cpp
 */

#include "comlet_ps.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

ComletPs::ComletPs() : ComletBase("ps", 
    "List status of processes.") {
}

void ComletPs::setupOptions() {
    this->opts = std::unique_ptr<ComletPsOptions>(new ComletPsOptions());
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

std::unique_ptr<nlohmann::json> ComletPs::run() {
    std::unique_ptr<nlohmann::json> json;
    if (this->opts->deviceId == "-1") {
        json = this->coreStub->getAllDeviceUtilizationByProcess(200 * 1000);
    } else {
        int targetId = -1;
        if (isNumber(this->opts->deviceId)) {
            targetId = std::stoi(this->opts->deviceId);
        } else {
            auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
            if (convertResult->contains("error")) {
                return convertResult;
            }
        }
        json = this->coreStub->getDeviceUtilizationByProcess(targetId, 200 * 1000);
    }
    return json;
}

void ComletPs::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    std::cout 
        << std::left << std::setfill(' ') 
        << std::setw(10) << "PID" 
        << std::setw(20) << "Command"
        << std::setw(15) << "DeviceID"
        << std::setw(15) << "SHR"
        << std::setw(15) << "MEM"
        << std::endl;
    for (auto iter = (*json)["device_util_by_proc_list"].begin(); 
            iter != (*json)["device_util_by_proc_list"].end(); iter++) {
        std::cout 
            << std::left << std::setfill(' ') 
            << std::setw(10) << (*iter)["process_id"].get<uint32_t>()
            << std::setw(20) << (*iter)["process_name"].get<std::string>()
            << std::setw(15) << (*iter)["device_id"].get<uint32_t>()
            << std::setprecision(4)
            << std::setw(15) << (*iter)["shared_mem_size"].get<uint64_t>() 
            << std::setw(15) << (*iter)["mem_size"].get<uint64_t>() 
            << std::endl;
    } 
}

} // end namespace xpum::cli
