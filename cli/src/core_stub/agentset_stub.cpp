/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file agentset_stub.cpp
 */

#include <nlohmann/json.hpp>
#include <string>

#include "lib_core_stub.h"
#include "logger.h"
#include "xpum_api.h"

namespace xpum::cli {

enum ValueType {
    VALUE_TYPE_INT64,
    VALUE_TYPE_DOUBLE,
    VALUE_TYPE_STRING,
};

struct AgentConfigType {
    xpum_agent_config_t key;
    std::string keyStr;
    ValueType valueType;
    std::string jsonFieldName;
};

static AgentConfigType agentConfigTypeList[]{
    {XPUM_AGENT_CONFIG_SAMPLE_INTERVAL, "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL", VALUE_TYPE_INT64, "sampling_interval"}};


std::unique_ptr<nlohmann::json> LibCoreStub::setAgentConfig(std::string jsonName, void* pValue) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAgentConfig() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}
} // namespace xpum::cli