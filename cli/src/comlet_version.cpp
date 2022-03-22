/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_version.cpp
 */

#include "comlet_version.h"

#include <map>
#include <nlohmann/json.hpp>

#include "config.h"
#include "core_stub.h"

namespace xpum::cli {

void ComletVersion::setupOptions() {
    this->opts = std::unique_ptr<ComletVersionOptions>(new ComletVersionOptions());
}

std::unique_ptr<nlohmann::json> ComletVersion::run() {
    auto json = this->coreStub->getVersion();
    (*json)["cli_version"] = CLI_VERSION;
    (*json)["cli_version_git"] = CLI_VERSION_GIT_COMMIT;
    return json;
}

void ComletVersion::getTableResult(std::ostream &out) {
    auto res = run();
    out << "CLI:" << std::endl;
    out << "    Version: " << res->value("cli_version", "") << std::endl;
    out << "    Build ID: " << res->value("cli_version_git", "") << std::endl;
    out << std::endl;
    out << "Service:" << std::endl;
    out << "    Version: " << res->value("xpum_version", "") << std::endl;
    out << "    Build ID: " << res->value("xpum_version_git", "") << std::endl;
    out << "    Level Zero Version: " << res->value("level_zero_version", "") << std::endl;
}
} // end namespace xpum::cli
