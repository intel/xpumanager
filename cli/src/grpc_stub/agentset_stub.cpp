/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file agentset_stub.cpp
 */

#include <nlohmann/json.hpp>
#include <string>

#include "core.grpc.pb.h"
#include "core.pb.h"
#include "grpc_core_stub.h"
#include "logger.h"
#include "exit_code.h"

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

static AgentConfigType* getAgentConfigTypeFromJsonName(std::string name) {
    for (auto& item : agentConfigTypeList) {
        if (item.jsonFieldName.compare(name) == 0) {
            return &item;
        }
    }
    return nullptr;
}

static AgentConfigType* getAgentConfigTypeFromKeyStr(std::string keyStr) {
    for (auto& item : agentConfigTypeList) {
        if (item.keyStr.compare(keyStr) == 0) {
            return &item;
        }
    }
    return nullptr;
}

static std::unique_ptr<nlohmann::json> getAgentConfigJsonOBject(const google::protobuf::RepeatedPtrField<AgentConfigEntry>& entries) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    for (auto entry : entries) {
        auto pConfigType = getAgentConfigTypeFromKeyStr(entry.key());
        auto value = entry.value();
        auto valueCase = value.value_case();
        std::string jsonName = pConfigType->jsonFieldName;
        switch (valueCase) {
            case FlexTypeValue::kIntValue: {
                int64_t intValue = value.intvalue();
                (*json)[jsonName] = intValue;
                break;
            }
            case FlexTypeValue::kFloatValue: {
                double floatValue = value.floatvalue();
                (*json)[jsonName] = floatValue;
                break;
            }
            case FlexTypeValue::kStringValue: {
                std::string strValue = value.stringvalue();
                (*json)[jsonName] = strValue;
                break;
            }
            case FlexTypeValue::VALUE_NOT_SET:
                break;
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::setAgentConfig(std::string jsonName, void* pValue) {
    assert(this->stub != nullptr);

    grpc::ClientContext context;
    SetAgentConfigRequest request;
    SetAgentConfigResponse response;

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    // prepare request
    auto pConfigType = getAgentConfigTypeFromJsonName(jsonName);

    auto entry = request.add_configentries();

    entry->set_key(pConfigType->keyStr);

    auto flexTypeValue = entry->mutable_value();

    switch (pConfigType->valueType) {
        case VALUE_TYPE_INT64:
            flexTypeValue->set_intvalue(*((int64_t*)pValue));
            XPUM_LOG_AUDIT("Set agent %s to value %d", pConfigType->keyStr.c_str(), flexTypeValue->intvalue());
            break;
        case VALUE_TYPE_DOUBLE:
            flexTypeValue->set_floatvalue(*((double*)pValue));
            XPUM_LOG_AUDIT("Set agent %s to value %f", pConfigType->keyStr.c_str(), flexTypeValue->floatvalue());
            break;
        case VALUE_TYPE_STRING:
            flexTypeValue->set_stringvalue(std::string((char*)pValue));
            XPUM_LOG_AUDIT("Set agent %s to value %s", pConfigType->keyStr.c_str(), flexTypeValue->stringvalue().c_str());
            break;
    }

    grpc::Status status = stub->setAgentConfig(&context, request, &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    if (response.errorlist().size() > 0) {
        auto error = response.errorlist()[0];
        auto pConfigType = getAgentConfigTypeFromKeyStr(error.key());
        (*json)["error"] = pConfigType->jsonFieldName + ":" + error.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    return getAgentConfigJsonOBject(response.entrylist().configentries());
}

std::unique_ptr<nlohmann::json> GrpcCoreStub::getAgentConfig() {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    grpc::ClientContext context;
    GetAgentConfigResponse response;

    grpc::Status status = stub->getAgentConfig(&context, google::protobuf::Empty(), &response);

    if (!status.ok()) {
        (*json)["error"] = status.error_message();
        (*json)["errno"] = XPUM_CLI_ERROR_GENERIC_ERROR;
        return json;
    }

    if (response.errormsg().length() != 0) {
        (*json)["error"] = response.errormsg();
        (*json)["errno"] = errorNumTranslate(response.errorno());
        return json;
    }

    return getAgentConfigJsonOBject(response.entrylist().configentries());
}
} // namespace xpum::cli