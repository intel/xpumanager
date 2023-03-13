/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_version.cpp
 */

#include "comlet_version.h"

#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"

void ComletVersion::setupOptions() {
    this->opts = std::unique_ptr<ComletVersionOptions>(new ComletVersionOptions());
}

std::unique_ptr<nlohmann::json> ComletVersion::run() {
    auto json = this->coreStub->getVersion();
    (*json)["cli_version"] = "1.2";
    (*json)["cli_version_git"] = "1.2";
    return json;
}

void ComletVersion::getTableResult(std::ostream& out) {
    auto res = run();
    out << "CLI:" << std::endl;
    out << "    Version: " << res->value("cli_version", "") << std::endl;
    out << "    Build ID: " << res->value("cli_version_git", "") << std::endl;
    // out << std::endl;
    // out << "Service:" << std::endl;
    // out << "    Version: " << res->value("xpum_version", "") << std::endl;
    // out << "    Build ID: " << res->value("xpum_version_git", "") << std::endl;
    out << "    Level Zero Version: " << res->value("level_zero_version", "") << std::endl;
}
