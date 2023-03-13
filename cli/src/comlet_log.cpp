/* 
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_log.cpp
 */

#include "comlet_log.h"

#include <linux/limits.h>
#include <map>
#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

ComletLog::ComletLog() : ComletBase("log", 
    "Collect GPU debug logs.") {
    printHelpWhenNoArgs = true;
}

void ComletLog::setupOptions() {
    this->opts = std::unique_ptr<ComletLogOptions>(new ComletLogOptions());
    addOption("-f,--file", this->opts->fileName, "The file (a tar.gz) to archive all the debug logs");
}

std::unique_ptr<nlohmann::json> ComletLog::run() {
    std::unique_ptr<nlohmann::json> json(new nlohmann::json());
    return this->coreStub->genDebugLog(this->opts->fileName);
}

void ComletLog::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    out << "Done" << std::endl;
    return; 
}

} // end namespace xpum::cli
