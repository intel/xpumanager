/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file redfish_amc_manager.cpp
 */

#include "amc/redfish_amc_manager.h"

#include <regex>

#include "hpe_redfish_amc_manager.h"
#include "infrastructure/logger.h"
#include "smc_redfish_amc_manager.h"
#include "util.h"

namespace xpum {
std::shared_ptr<AmcManager> RedfishAmcManager::instance() {
    std::string output;
    doCmd("dmidecode -t system", output);

    std::regex manufacturerPattern("Manufacturer\\: (.*)");
    std::smatch sm;
    std::string manufacturer;
    if (std::regex_search(output, sm, manufacturerPattern)) {
        manufacturer = sm[1].str();
    }
    if (manufacturer == "HPE") {
        return std::make_shared<HEPRedfishAmcManager>();
    }else{
        return std::make_shared<SMCRedfishAmcManager>();
    }
}

std::string getRedfishAmcWarn() {
    std::string output;
    doCmd("dmidecode -t system", output);

    std::regex manufacturerPattern("Manufacturer\\: (.*)");
    std::smatch sm;
    std::string manufacturer;
    if (std::regex_search(output, sm, manufacturerPattern)) {
        manufacturer = sm[1].str();
    }
    if (manufacturer == "HPE") {
        return HEPRedfishAmcManager::getRedfishAmcWarn();
    } else if (manufacturer == "Supermicro") {
        return SMCRedfishAmcManager::getRedfishAmcWarn();
    } else {
        return "";
    }
}

} // namespace xpum