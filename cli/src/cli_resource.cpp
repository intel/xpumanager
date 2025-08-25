/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_resource.cpp
 */

#include <string>
#include <unordered_map>
#include "config.h"

namespace xpum::cli {

namespace {
#ifdef DAEMONLESS
std::unordered_map<std::string, std::string> string_table = {
    {"CLI_APP_DESC", "Intel XPU System Management Interface -- v" CLI_VERSION_IN_HELP "\nIntel XPU System Management Interface provides the Intel datacenter GPU model. It can also be used to update the firmware.\nIntel XPU System Management Interface is based on Intel oneAPI Level Zero. Before using Intel XPU System Management Interface, the GPU driver and Intel oneAPI Level Zero should be installed rightly.\n\nSupported devices:\n - Intel Data Center GPU"}};
#else
std::unordered_map<std::string, std::string> string_table = {
    {"CLI_APP_DESC", "Intel XPU Manager Command Line Interface -- v" CLI_VERSION_IN_HELP "\nIntel XPU Manager Command Line Interface provides the Intel data center GPU model and monitoring capabilities. It can also be used to change the Intel data center GPU settings and update the firmware.\nIntel XPU Manager is based on Intel oneAPI Level Zero. Before using Intel XPU Manager, the GPU driver and Intel oneAPI Level Zero should be installed rightly.\n\nSupported devices:\n  - Intel Data Center GPU "}};
#endif

} // namespace

const std::string&
getResourceString(const std::string& key) {
    auto search = string_table.find(key);
    if (search != string_table.end()) {
        return search->second;
    } else {
        return key;
    }
}

} // namespace xpum::cli