/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_version.cpp
 */

#include "comlet_version.h"

#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "core_stub.h"
#include "resource.h"

void ComletVersion::setupOptions() {
    this->opts = std::unique_ptr<ComletVersionOptions>(new ComletVersionOptions());
}

std::unique_ptr<nlohmann::json> ComletVersion::run() {
    auto json = this->coreStub->getVersion();
    (*json)["cli_version"] = VER_VERSION_MAJORMINORPATCH_STR;
    std:string cli_version_git = VER_COMMIT_VERSION;
    (*json)["cli_version_git"] = cli_version_git.substr(0,8);
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
