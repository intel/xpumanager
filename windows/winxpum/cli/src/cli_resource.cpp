/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_resource.cpp
 */

#include <string>
#include <unordered_map>"

#include "../resource.h"

namespace xpum::cli {

    namespace {
        std::string CLI_VERSION_IN_HELP = std::to_string(VER_VERSION_MAJOR) + "." + std::to_string(VER_VERSION_MINOR);
        std::unordered_map<std::string, std::string> string_table = {
                {"CLI_APP_DESC", "Intel XPU System Management Interface -- v" + CLI_VERSION_IN_HELP + "\nIntel XPU System Management Interface provides the Intel datacenter GPU model. It can also be used to update the firmware.\nIntel XPU System Management Interface is based on Intel oneAPI Level Zero. Before using Intel XPU System Management Interface, the GPU driver and Intel oneAPI Level Zero should be installed rightly.\n\nSupported devices:\n - Intel Data Center GPU"}};

    } // namespace

    const std::string&
        getResourceString(const std::string& key) {
        auto search = string_table.find(key);
        if (search != string_table.end()) {
            return search->second;
        }
        else {
            return key;
        }
    }

} // namespace xpum::cli