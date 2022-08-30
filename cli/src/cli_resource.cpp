/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_resource.cpp
 */

#include <string>
#include <unordered_map>

namespace xpum::cli {

namespace {
std::unordered_map<std::string, std::string> string_table = {
    {"CLI_APP_DESC", "Intel XPU System Management Interface -- v1.0\nIntel XPU System Management Interface provides the Intel datacenter GPU model. It can also be used to update the firmware.\nIntel XPU System Management Interface is based on Intel oneAPI Level Zero. Before using Intel XPU System Management Interface, the GPU driver and Intel oneAPI Level Zero should be installed rightly.\n\nSupported devcies:\n - Intel Data Center GPU"}};

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