/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file main.cpp
 */

#include <string>
#include <vector>
#include <iostream>

#include "CLI/App.hpp"
#include <CLI/CLI.hpp>
#include "amc/ipmi_amc_manager.h"

std::string filePath;

namespace xpum {

extern int cmd_firmware(const char* file, unsigned int versions[4]);

extern int cmd_get_amc_firmware_versions(int buf[][4], int* count);

extern void setPercentCallbackAndContext(percent_callback_func_t callback, void* pAmcManager);
} // namespace xpum

int listAmcFwVersions() {
    std::vector<std::string> versions;
    int count;
    int err = xpum::cmd_get_amc_firmware_versions(nullptr, &count);
    if (err != 0 || count <= 0) {
        std::cout << "No AMC found" << std::endl;
        return err;
    }
    int buf[count][4];
    err = xpum::cmd_get_amc_firmware_versions(buf, &count);
    if (err != 0) {
        std::cout << "No AMC found" << std::endl;
        return err;
    }
    for (int i = 0; i < count; i++) {
        std::stringstream ss;
        ss << buf[i][0] << ".";
        ss << buf[i][1] << ".";
        ss << buf[i][2] << ".";
        ss << buf[i][3];
        versions.push_back(ss.str());
    }
    std::cout << versions.size() << " AMC are found" << std::endl;
    int i = 0;
    for (auto version : versions) {
        std::cout << "AMC " << i++ << " firmware version: " << version << std::endl;
    }
    return 0;
}

int updateAmcFw() {
    std::cout << "Updating" << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    CLI::App app{};

    CLI::App* fwversion = app.add_subcommand("fwversion", "list all AMC firmware versions");
    CLI::App* updatefw = app.add_subcommand("updatefw", "update all ATSM AMC firmware to the specified version");
    updatefw->add_option("-f", filePath, "AMC firmware filename");

    app.require_subcommand(0, 1);

    CLI11_PARSE(app, argc, argv);

    if (fwversion->parsed()) {
        return listAmcFwVersions();
    } else if (updatefw->parsed()) {
        return updateAmcFw();
    }
}