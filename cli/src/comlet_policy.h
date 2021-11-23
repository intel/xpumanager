#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"
#include "core.grpc.pb.h"
#include "core.pb.h"

namespace xpum::cli {

struct ComletPolicyOptions {
    bool listAll = false;
    bool listAllPolicyType = false;
    bool create = false;
    bool remove = false;
    int deviceId = -1;
    int groupId = -1;
    std::string policyType = "";
    std::string policyConditionType = "";
    std::string policyActionType = "";
    int threshold = -2;
    double throttleDeviceFrequencyMin=-200000;
    double throttleDeviceFrequencyMax=-200000;
};

class ComletPolicy : public ComletBase {
   public:
    ComletPolicy() : ComletBase("policy", "Policy of the device") {}
    virtual ~ComletPolicy() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    XpumPolicyActionType policyActionTypeEnumFromString(std::string& type);
    XpumPolicyConditionType policyConditionTypeEnumFromString(std::string& type);
    XpumPolicyType policyTypeEnumFromString(std::string &type);

   private:
    std::unique_ptr<ComletPolicyOptions> opts;
};
} // end namespace xpum::cli
