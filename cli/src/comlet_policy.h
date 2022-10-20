/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_policy.h
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"
#include "xpum_structs.h"

namespace xpum::cli {

struct ComletPolicyOptions {
    bool listAll = false;
    bool listalltypes = false;
    bool create = false;
    bool remove = false;
    std::string deviceId = "-1";
    uint32_t groupId = 0;
    std::string policyType = "";
    std::string policyConditionType = "";
    std::string policyActionType = "";
    int threshold = -200000;
    double throttlefrequencymin = -200000;
    double throttlefrequencymax = -200000;
};

class ComletPolicy : public ComletBase {
   public:
    ComletPolicy() : ComletBase("policy", "Get and set the GPU policies.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletPolicy() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream& out) override;

    xpum_policy_action_type_t policyActionTypeEnumFromString(std::string& type);
    xpum_policy_conditon_type_t policyConditionTypeEnumFromString(std::string& type);
    xpum_policy_type_t policyTypeEnumFromString(std::string& type);

    bool isTypeConditionActionMatch();

    inline const bool isListSupportedTypes() const {
        return opts->listalltypes;
    }
    inline const bool isListAll() const {
        return opts->listAll;
    }
    inline const std::string getDeviceId() const {
        return opts->deviceId;
    }
    inline const int getGroupId() const {
        return opts->groupId;
    }

   private:
    std::unique_ptr<ComletPolicyOptions> opts;

    bool isDeviceValid(int deviceId);
    bool isGroupValid(int groupId);
};
} // end namespace xpum::cli
