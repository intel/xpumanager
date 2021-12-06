#include <string>

#include "xpum_api.h"
#include "xpum_core_service_impl.h"

namespace xpum::daemon {

static int agentConfigStrToKey(std::string keyStr) {
    if (keyStr.compare("XPUM_AGENT_CONFIG_SAMPLE_INTERVAL")) {
        return XPUM_AGENT_CONFIG_SAMPLE_INTERVAL;
    }
    return -1;
}

static std::string agentConfigKeyToStr(xpum_agent_config_t key) {
    switch (key) {
        case XPUM_AGENT_CONFIG_SAMPLE_INTERVAL:
            return "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL";
        default:
            return "";
    }
}

void static fillAgentConfigReponse(::AgentConfigResponse* response) {
    int64_t intValue;
    xpum_agent_config_t key = XPUM_AGENT_CONFIG_SAMPLE_INTERVAL;
    xpumGetAgentConfig(key, &intValue);
    auto entry = response->add_configentries();
    entry->set_key(agentConfigKeyToStr(key));
    auto value = entry->mutable_value();
    value->set_intvalue(intValue);
}

::grpc::Status XpumCoreServiceImpl::setAgentConfig(::grpc::ServerContext* context, const ::AgentConfigMessage* request, ::AgentConfigResponse* response) {
    auto keyStr = request->key();
    int key = agentConfigStrToKey(keyStr);
    if (key == -1) {
        response->set_status(1);
        response->set_errormsg("Unknown agent config key");
        return grpc::Status::OK;
    }
    auto value = request->value();
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
        response->set_errormsg("No value passed");
        response->set_status(1);
        return grpc::Status::OK;
    }
    auto res = xpumSetAgentConfig((xpum_agent_config_t)key, p);
    if (res != XPUM_OK) {
        response->set_errormsg("Error");
        response->set_status(1);
        return grpc::Status::OK;
    }
    // fill response config entries
    fillAgentConfigReponse(response);
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getAgentConfig(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::AgentConfigResponse* response) {
    fillAgentConfigReponse(response);
    return grpc::Status::OK;
}
} // namespace xpum::daemon