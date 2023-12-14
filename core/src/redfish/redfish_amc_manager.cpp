/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file redfish_amc_manager.cpp
 */

#include "amc/redfish_amc_manager.h"

#include <regex>
#include <fstream>
#include <sys/stat.h>

#include "hpe_redfish_amc_manager.h"
#include "infrastructure/logger.h"
#include "smc_redfish_amc_manager.h"
#include "dell_redfish_amc_manager.h"
#include "intel_dnp_redfish_amc_manager.h"
#include "lenovo_florence_redfish_amc_manager.h"
#include "util.h"
#include "infrastructure/xpum_config.h"
#include "infrastructure/configuration.h"

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
    } else if (manufacturer == "Dell Inc.") {
        return std::make_shared<DELLRedfishAmcManager>();
    } else if (manufacturer == "Intel Corporation") {
        return std::make_shared<DenaliPassRedfishAmcManager>();
    } else if (manufacturer == "Lenovo") {
        return std::make_shared<FlorenceRedfishAmcManager>();
    } else {
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
    } else if (manufacturer == "Dell Inc.") {
        return DELLRedfishAmcManager::getRedfishAmcWarn();
    } else if (manufacturer == "Intel Corporation") {
        return DenaliPassRedfishAmcManager::getRedfishAmcWarn();
    } else if (manufacturer == "Lenovo") {
        return FlorenceRedfishAmcManager::getRedfishAmcWarn();
    } else {
        return "";
    }
}

#define XPUM_CURL_TIMEOUT_DEFAULT 120L

int XPUM_CURL_TIMEOUT = XPUM_CURL_TIMEOUT_DEFAULT;

void RedfishAmcManager::readConfigFile(){
    XPUM_CURL_TIMEOUT = XPUM_CURL_TIMEOUT_DEFAULT;
    std::string file_name = "xpum.conf";
    std::string file_path = std::string(XPUM_CONFIG_DIR) + file_name;
    struct stat buffer;
    if (stat(file_path.c_str(), &buffer) != 0) {
        char exePath[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
        if (len < 0) {
            XPUM_LOG_ERROR("couldn't read link : {}", exePath);
            len = 0;
        }
        if (len >= PATH_MAX) {
            len = PATH_MAX - 1;
        }
        exePath[len] = '\0';
        std::string current_file = exePath;
        file_path = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/" + Configuration::getXPUMMode() + "/config/" + file_name;
        if (stat(file_path.c_str(), &buffer) != 0)
            file_path = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/" + Configuration::getXPUMMode() + "/config/" + file_name;
    }
    std::ifstream conf_file(file_path);
    if (!conf_file.is_open()) { 
        XPUM_LOG_ERROR("couldn't open config file : {}", file_path);
        return ;
    }
    std::string line;
    while (getline(conf_file, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
        if (line[0] == '#' || line.empty())
            continue;
        auto delimiter_pos = line.find("=");
        auto name = line.substr(0, delimiter_pos);
        auto value = line.substr(delimiter_pos + 1);
        if (value.find("#") != std::string::npos)
            value = value.substr(0, value.find("#"));
        if (name == "REDFISH_HOST_TIMEOUT") {
            try {
                int v = atoi(value.c_str());
                if (v > 0) {
                    XPUM_CURL_TIMEOUT = v;
                    XPUM_LOG_INFO("REDFISH_HOST_TIMEOUT set to: {}", XPUM_CURL_TIMEOUT);
                }
            } catch (std::exception &e) {
                XPUM_LOG_ERROR("Get invalid value for REDFISH_HOST_TIMEOUT: {}", value);
            }
        }
    }
    conf_file.close();
}

} // namespace xpum