#include <string>

#include "xpum_api.h"
#include "xpum_core_service_impl.h"

namespace xpum::daemon {

enum AgentConfigFlexValueType {
    VALUE_TYPE_INT64,
    VALUE_TYPE_DOUBLE,
    VALUE_TYPE_STRING
};

struct AgentConfigKey {
    xpum_agent_config_t key;
    AgentConfigFlexValueType type;
    std::string keyStr;
};

static AgentConfigKey agentConfigKeys[]{
    {XPUM_AGENT_CONFIG_SAMPLE_INTERVAL, VALUE_TYPE_INT64, "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL"}};

static int agentConfigStrToKey(std::string keyStr) {
    for (auto config : agentConfigKeys) {
        if (config.keyStr.compare(keyStr) == 0) {
            return config.key;
        }
    }
    return -1;
}

void static fillAgentConfigReponse(::AgentConfigEntryList* response) {
    int64_t intValue;
    double floatValue;
    char stringValue[256];
    for (AgentConfigKey config : agentConfigKeys) {
        auto key = config.key;
        auto type = config.type;
        auto keyStr = config.keyStr;
        auto entry = response->add_configentries();
        entry->set_key(keyStr);
        auto flexValue = entry->mutable_value();
        switch (type) {
            case VALUE_TYPE_INT64:
                xpumGetAgentConfig(key, &intValue);
                flexValue->set_intvalue(intValue);
                break;
            case VALUE_TYPE_DOUBLE:
                xpumGetAgentConfig(key, &floatValue);
                flexValue->set_floatvalue(floatValue);
                break;
            case VALUE_TYPE_STRING:
                xpumGetAgentConfig(key, stringValue);
                std::string str(stringValue);
                flexValue->set_stringvalue(str);
                break;
        }
    }
}

::grpc::Status XpumCoreServiceImpl::setAgentConfig(::grpc::ServerContext* context, const ::SetAgentConfigRequest* request, ::SetAgentConfigResponse* response) {
    auto entries = request->configentries();
    for (auto entry : entries) {
        auto keyStr = entry.key();
        int key = agentConfigStrToKey(keyStr);
        if (key == -1) {
            auto error = response->add_errorlist();
            error->set_key(keyStr);
            error->set_errormsg("Unknown agent config key");
            continue;
        }
        auto value = entry.value();
        auto valueCase = value.value_case();
        void* p = nullptr;
        switch (valueCase) {
            case FlexTypeValue::kIntValue: {
                int64_t intValue = value.intvalue();
                p = (void*)&intValue;
                break;
            }
            case FlexTypeValue::kFloatValue: {
                double floatValue = value.floatvalue();
                p = (void*)&floatValue;
                break;
            }
            case FlexTypeValue::kStringValue: {
                std::string strValue = value.stringvalue();
                p = (void*)strValue.c_str();
                break;
            }
            case FlexTypeValue::VALUE_NOT_SET:
                break;
        }
        if (p == nullptr) {
            auto error = response->add_errorlist();
            error->set_key(keyStr);
            error->set_errormsg("No value passed");
            continue;
        }
        auto res = xpumSetAgentConfig((xpum_agent_config_t)key, p);
        if (res != XPUM_OK) {
            auto error = response->add_errorlist();
            error->set_key(keyStr);
            error->set_errormsg("Error: fail to set");
        }
    }

    // fill response config entries
    fillAgentConfigReponse(response->mutable_entrylist());

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getAgentConfig(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::GetAgentConfigResponse* response) {
    fillAgentConfigReponse(response->mutable_entrylist());
    return grpc::Status::OK;
}
} // namespace xpum::daemon