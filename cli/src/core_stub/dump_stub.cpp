/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_stub.cpp
 */

#include <nlohmann/json.hpp>
#include <vector>

#include "logger.h"
#include "xpum_api.h"
#include "lib_core_stub.h"

namespace xpum::cli {

std::unique_ptr<nlohmann::json> LibCoreStub::startDumpRawDataTask(uint32_t deviceId, int tileId, std::vector<xpum_dump_type_t> dumpTypeList) {

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    return json;
}
std::unique_ptr<nlohmann::json> LibCoreStub::stopDumpRawDataTask(int taskId) {

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    return json;
}
std::unique_ptr<nlohmann::json> LibCoreStub::listDumpRawDataTasks() {

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    return json;
}

} // namespace xpum::cli