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
    {"CLI_APP_DESC", "Intel XPU Manager Command Line Interface -- v1.0\nIntel XPU Manager Command Line Interface provides the Intel datacenter GPU model and monitoring capabilities. It can also be used to change the Intel datacenter GPU settings and update the firmware.\nIntel XPU Manager is based on Intel oneAPI Level Zero. Before using Intel XPU Manager, the GPU driver and Intel oneAPI Level Zero should be installed rightly.\n\nSupported devcies:\n- Intel ATS-M1/ATS-M3/ATS-P"}};

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