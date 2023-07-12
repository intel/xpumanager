/*
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file helper.cpp
 */

#include "helper.h"
#include "diagnostic_manager.h"
#include "precheck.h"
#include "infrastructure/xpum_config.h"
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <algorithm>

namespace xpum {

    bool isPathExist(const std::string &s) {
        struct stat buffer;
        return (stat(s.c_str(), &buffer) == 0);
    }
    
    void readConfigFile() {
        DiagnosticManager::thresholds.clear();
        std::string file_name = std::string(XPUM_CONFIG_DIR) + std::string("diagnostics.conf");
        if (!isPathExist(file_name)) {
            char exe_path[XPUM_MAX_PATH_LEN];
            ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
            exe_path[len] = '\0';
            std::string current_file = exe_path;
            std::string xpum_mode = "xpum";
            if (current_file.find_last_of('/') != std::string::npos)
            xpum_mode = current_file.substr(current_file.find_last_of('/') + 1);
            if (xpum_mode != "xpu-smi") {
                file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/xpum/config/" + std::string("diagnostics.conf");
                if (!isPathExist(file_name))
                    file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/xpum/config/" + std::string("diagnostics.conf");
            } else {
                file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/xpu-smi/config/" + std::string("diagnostics.conf");
                if (!isPathExist(file_name))
                    file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/xpu-smi/config/" + std::string("diagnostics.conf");
            }
        }
        std::ifstream conf_file(file_name);
        if (conf_file.is_open()) {
            XPUM_LOG_DEBUG("read config for diagnostics and precheck from file: {}", file_name);
            std::string line;
            std::string current_device;
            while (getline(conf_file, line)) {
                line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
                if (line[0] == '#' || line.empty())
                    continue;
                auto delimiter_pos = line.find("=");
                auto name = line.substr(0, delimiter_pos);
                auto value = line.substr(delimiter_pos + 1);
                if (value.find("#") != std::string::npos)
                    value = value.substr(0, value.find("#"));
                if (name == "CPU_TEMPERATURE_THRESHOLD") {
                    PrecheckManager::cpu_temperature_threshold = atoi(value.c_str());
                } else if (name == "MEDIA_CODER_TOOLS_PATH") {
                    if (value == "/usr/bin/" || value == "/usr/share/mfx/samples/") {
                        DiagnosticManager::MEDIA_CODER_TOOLS_PATH = value;
                    }
                } else if (name == "MEDIA_CODER_TOOLS_1080P_FILE") {
                    DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE = value;
                } else if (name == "MEDIA_CODER_TOOLS_4K_FILE") {
                    DiagnosticManager::MEDIA_CODER_TOOLS_4K_FILE = value;
                } else if (name == "ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT") {
                    try {
                        int val = std::stoi(value);
                        if (val > 0)
                            DiagnosticManager::ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT = val;
                    } catch(...) { }
                } else if (name == "MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST") {
                    try {
                        float val = std::stof(value);
                        if (val > 0 && val <= 1)
                            DiagnosticManager::MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST = val;
                    } catch(...) { }
                } else if (name == "XE_LINK_THROUGHPUT_USAGE_PERCENTAGE") {
                    try {
                        float val = std::stof(value);
                        if (val > 0 && val <= 1)
                            DiagnosticManager::XE_LINK_THROUGHPUT_USAGE_PERCENTAGE = val;
                    } catch(...) { }
                } else if (name == "NAME") {
                    current_device = value;
                } else {
                    DiagnosticManager::thresholds[current_device][name] = atoi(value.c_str());
                }
            }
            conf_file.close();
        } else {
            XPUM_LOG_ERROR("couldn't open config file for diagnostics and precheck: {}", file_name);
        }    
    }
} // namespace xpum
