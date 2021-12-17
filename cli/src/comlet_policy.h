#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"
#include "core.grpc.pb.h"
#include "core.pb.h"

namespace xpum::cli {

struct ComletPolicyOptions {
    bool listAll = false;
    bool listalltypes = false;
    bool create = false;
    bool remove = false;
    int deviceId = -1;
    int groupId = -1;
    std::string policyType = "";
    std::string policyConditionType = "";
    std::string policyActionType = "";
    int threshold = -2;
    double throttlefrequencymin=-200000;
    double throttlefrequencymax=-200000;
};

class ComletPolicy : public ComletBase {
   public:
    //ComletPolicy() : ComletBase("policy", "Manager GPU policies.\n\nUsage: xpumcli policy [Options]\n\txpumcli policy -d [deviceId] -l\n\txpumcli policy -d [deviceId] -l -j\n\txpumcli policy -g [groupId] -l\n\txpumcli policy -g [groupId] -l -j\n\txpumcli policy -c -d [deviceId] --type [policyTypeValue] --condition 1 --threshold [threshold]  --action [policyActionValue]\n\txpumcli policy -c -d [deviceId] --type [policyTypeValue] --condition 2 --action [policyActionValue]\n\txpumcli policy -c -g [groupId] --type 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]\n\txpumcli policy -r -d [deviceId] --type [policyTypeValue]\n\txpumcli policy -r -g [groupId] --type [policyTypeValue]\n\n") {}
    ComletPolicy() : ComletBase("policy", "Manager GPU policies.") {}
    virtual ~ComletPolicy() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    XpumPolicyActionType policyActionTypeEnumFromString(std::string& type);
    XpumPolicyConditionType policyConditionTypeEnumFromString(std::string& type);
    XpumPolicyType policyTypeEnumFromString(std::string &type);

    bool isTypeConditionActionMatch();

    inline const bool isListSupportedTypes() const {
        return opts->listalltypes;
    }
    inline const bool isListAll() const {
        return opts->listAll;
    }
    inline const int getDeviceId() const {
        return opts->deviceId;
    }
    inline const int getGroupId() const {
        return opts->groupId;
    }

   private:
    std::unique_ptr<ComletPolicyOptions> opts;
};
} // end namespace xpum::cli
