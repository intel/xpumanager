/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file main.cpp
 */

#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "CLI/App.hpp"
#include "CLI/CLI.hpp"
#include "CLI/Formatter.hpp"
#include "amc/ipmi_amc_manager.h"
#include "config.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include "ipmi/ipmi.h"

std::string filePath;
bool assumeyes;

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

static void printProgress(int percentage) {
    int barWidth = 60;

    std::cout << "[";
    int pos = barWidth * (percentage / 100.0);
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    std::cout << "] " << percentage << " %\r";
    std::cout.flush();
}

static void percent_callback(uint32_t percent, void* pAmcManager) {
    printProgress(percent);
}

int updateAmcFw() {
    xpum::setPercentCallbackAndContext(percent_callback, nullptr);

    int rc = xpum::cmd_firmware(filePath.c_str(), nullptr);

    if (rc == 0) {
        printProgress(100);
        std::cout << std::endl;
        std::cout << "Update firmware successfully." << std::endl;
    } else {
        auto errMsg = xpum::getIpmiErrorString(rc);
        std::cout << std::endl;
        std::cout << "Error: " << errMsg << std::endl;
    }
    return rc;
}

class HelpFormatter : public CLI::Formatter {
   public:
    std::string make_option_opts(const CLI::Option*) const override {
        return "";
    }
};

int main(int argc, char** argv) {

    putenv(const_cast<char*>("SPDLOG_LEVEL=OFF"));

    spdlog::cfg::load_env_levels();

    CLI::App app{};

    CLI::Option* versionFlag = app.add_flag_callback(
        "-v, --version", []() {
            std::cout<<"Version: "<< AMCMCLI_VERSION << std::endl;
            std::cout<<"Build ID: "<< AMCMCLI_BUILD_ID << std::endl;
        },
        "List version info");

    CLI::App* fwversion = app.add_subcommand("fwversion", "List all AMC firmware versions");

    CLI::App* updatefw = app.add_subcommand("updatefw", "Update all ATSM AMC firmware to the specified version");
    updatefw->formatter(std::make_shared<HelpFormatter>());

    updatefw->add_flag("-y, --assumeyes", assumeyes, "Assume that the answer to any question which would be asked is yes");

    auto filePathOption = updatefw->add_option("-f", filePath, "AMC firmware filename");
    filePathOption->required();
    filePathOption->transform([](const std::string& str) {
        if (FILE* file = fopen(str.c_str(), "r")) {
            fclose(file);
            // get full path of firmware image path
            char resolved_path[PATH_MAX];
            char* fullpath = realpath(str.c_str(), resolved_path);
            return std::string(fullpath);
        } else {
            throw CLI::ValidationError("Invalid file path.");
        }
    });

    app.require_subcommand(0, 1);

    CLI11_PARSE(app, argc, argv);

    if (fwversion->parsed()) {
        return listAmcFwVersions();
    } else if (updatefw->parsed()) {
        std::cout << "CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model." << std::endl;
        std::cout << "Please confirm to proceed (y/n) ";
        if (!assumeyes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                std::cout << "update aborted" << std::endl;
                return 1;
            }
        } else {
            std::cout << std::endl;
        }
        std::cout << "Start to update firmware" << std::endl;
        std::cout << "Firmware Name: AMC" << std::endl;
        std::cout << "Image path: " << filePath << std::endl;
        return updateAmcFw();
    } else if (!versionFlag->empty()) {
        return 0;
    } else {
        std::cout << app.help();
    }
}
