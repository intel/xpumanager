/*
 *  Copyright (C) 2021-2023 Intel Corporation
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

static int agentConfigStrToKey(std::string keyStr) {
    for (auto config : agentConfigTypeList) {
        if (config.keyStr.compare(keyStr) == 0) {
            return config.key;
        }
    }
    return -1;
}
std::unique_ptr<nlohmann::json> LibCoreStub::setAgentConfig(std::string jsonName, void* pValue) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    auto key = agentConfigStrToKey(jsonName);
    if(key == -1){
        (*json)["error"] = "Config Name is not found";
    }
    auto res = xpumSetAgentConfig((xpum_agent_config_t)key, pValue);

    switch (res)
    {
    case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
        (*json)["error"] = "Level Zero Initialization Error";
        break;
    case XPUM_NOT_INITIALIZED:
        (*json)["error"] = "XPUM is not initializaed";
        break;
    case XPUM_RESULT_UNKNOWN_AGENT_CONFIG_KEY:
        (*json)["error"] = "Unknow Agent Config Key";
        break;
    case XPUM_RESULT_AGENT_SET_INVALID_VALUE:
        (*json)["error"] = "Invalid Agent Set Value";
        break;

    default:
        (*json)["error"] = "Error";
        break;
    }

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getAgentConfig() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    return json;
}
} // namespace xpum::cli