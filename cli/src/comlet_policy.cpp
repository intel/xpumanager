/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_policy.cpp
 */

#include "comlet_policy.h"

#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"

namespace xpum::cli {

static CharTableConfig ComletConfigAllSupported(R"({
    "columns": [{
        "title": "Types"
    }, {
        "title": "Conditions"
    }, {
        "title": "Actions"
    }],
    "rows": [{
        "instance": "all_policy_type[]",
        "in_array_sep": false,
        "cells": [
            "type",
            "condition",
            "action"
        ]
    }]
})"_json);

static CharTableConfig ComletConfigListAll(R"({
    "columns": [{
        "title": "Device ID"
    }, {
        "title": "Types"
    }, {
        "title": "Conditions"
    }, {
        "title": "Actions"
    }],
    "rows": [{
        "instance": "all_policy_list[].policy_list[]",
        "cells": [
            "device_id",
            "type",
            "condition",
            "action"
        ]
    }]
})"_json);

static CharTableConfig ComletConfigListDevice(R"({
    "columns": [{
        "title": "Device ID"
    }, {
        "title": "Types"
    }, {
        "title": "Conditions"
    }, {
        "title": "Actions"
    }],
    "rows": [{
        "instance": "all_policy_list.policy_list[]",
        "cells": [
            "device_id",
            "type",
            "condition",
            "action"
        ]
    }]
})"_json);

void ComletPolicy::setupOptions() {
    this->opts = std::unique_ptr<ComletPolicyOptions>(new ComletPolicyOptions());
    addOption("-d,--device", this->opts->deviceId, "The device ID.");
    addOption("-g,--group", this->opts->groupId, "The group ID.\n");

    addFlag("-l,--list", this->opts->listAll, "List all policies.");
    addFlag("--listalltypes", this->opts->listalltypes, "List all policy types, including the supported condition and action.");
    addFlag("-c,--create", this->opts->create, "Create one policy.");
    addFlag("-r,--remove", this->opts->remove, "Remove one policy. Only the policy is removed and the changed GPU settings will not be resumed.\n");

    //addOption("--type", this->opts->policyType, "Policy types.\n\t1. GPU Core Temperature\n\t2. Programming Errors\n\t3. Driver Errors\n\t4. Cache Errors Correctable\n\t5. Cache Errors Uncorrectable");
    addOption("--type", this->opts->policyType, "Policy types.\n\t1. GPU Core Temperature");
    //addOption("--condition", this->opts->policyConditionType, "Conditions.\n\t1. More than\n\t2. When occur");
    addOption("--condition", this->opts->policyConditionType, "Conditions.\n\t1. More than");
    addOption("--threshold", this->opts->threshold, "Threshold");
    //addOption("--action", this->opts->policyActionType, "Policy action.\n\t1. Throttle GPU\n\t2. Reset GPU");
    addOption("--action", this->opts->policyActionType, "Policy action.\n\t1. Throttle GPU");
    addOption("--throttlefrequencymin", this->opts->throttlefrequencymin, "Throttle GPU frequency to min value");
    addOption("--throttlefrequencymax", this->opts->throttlefrequencymax, "Throttle GPU frequency to max value");
}

bool ComletPolicy::isDeviceValid(int deviceId) {
    // check deviceId and tileId is valid
    auto res = this->coreStub->getDeviceProperties(deviceId);
    if (res->contains("error")) {
        return false;
    }
    return true;
}
bool ComletPolicy::isGroupValid(int groupId) {
    // check deviceId and tileId is valid
    auto res = this->coreStub->groupList(groupId);
    if (res->contains("error")) {
        return false;
    }
    return true;
}

std::unique_ptr<nlohmann::json> ComletPolicy::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->listAll) {
        //std::cout << "--GangDebug----listAll---1---" << std::endl;
        bool isDevcie;
        int id;
        if (this->opts->deviceId != -1) {
            isDevcie = true;
            id = this->opts->deviceId;
            json = this->coreStub->getPolicyById(isDevcie, id);
        } else if (this->opts->groupId != -1) {
            isDevcie = false;
            id = this->opts->groupId;
            json = this->coreStub->getPolicyById(isDevcie, id);
        } else {
            json = this->coreStub->getAllPolicy();
        }
        return json;
    }
    if (this->opts->listalltypes) {
        json = this->coreStub->getAllPolicyType();
        return json;
    }
    if (this->opts->create) {
        if (this->opts->deviceId == -1 && this->opts->groupId == -1) {
            // (*json)["error"] = "Too many operation flags";
            // return json;
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <device> or <group> should be specified by -d or -g option";
            return json;
        }

        //
        bool isDevcie = true;
        int id = this->opts->deviceId;

        //std::cout << "--GangDebug--id=" << id << std::endl;
        if (this->opts->groupId != -1) {
            isDevcie = false;
            id = this->opts->groupId;
        }
        XpumPolicyData policy;
        policy.set_deviceid(id);
        if (this->opts->policyType.length() == 0) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <type> should be specified by --type option";
            return json;
        }
        if (!(this->opts->policyType == "1"
              // ||this->opts->policyType == "2"
              // ||this->opts->policyType == "3"
              // ||this->opts->policyType == "4"
              // ||this->opts->policyType == "5"
              )) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <type> is invalid";
            return json;
        }
        policy.set_type(this->policyTypeEnumFromString(this->opts->policyType));

        //
        if (this->opts->policyConditionType.length() == 0) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <condition> should be specified by --condition option";
            return json;
        } else {
            if (!(this->opts->policyConditionType == "1"
                  //||this->opts->policyConditionType == "2"
                  )) {
                (*json)["is_success"] = false;
                (*json)["error"] = "Wrong argument: <condition> is invalid";
                return json;
            }
            if (this->opts->policyConditionType == "1"    //"POLICY_CONDITION_TYPE_GREATER"
                || this->opts->policyConditionType == "3" //"POLICY_CONDITION_TYPE_LESS"
            ) {
                // if (this->opts->threshold == -200000) {
                //     (*json)["is_success"] = false;
                //     (*json)["error"] = "Wrong argument: <threshold> should be specified by --threshold option";
                //     return json;
                // }
                if (this->opts->threshold < 0) {
                    (*json)["is_success"] = false;
                    (*json)["error"] = "Wrong argument: <threshold> is invalid (not empty and greater than or equal 0)";
                    return json;
                }
                policy.mutable_condition()->set_threshold(this->opts->threshold);
            }
            policy.mutable_condition()->set_type(this->policyConditionTypeEnumFromString(this->opts->policyConditionType));
        }
        if (this->opts->policyActionType.length() == 0) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <action> should be specified by --action option";
            return json;
        } else {
            if (!(this->opts->policyActionType == "1"
                  //||this->opts->policyActionType == "2"
                  )) {
                (*json)["is_success"] = false;
                (*json)["error"] = "Wrong argument: <action> is invalid";
                return json;
            }
            if (this->opts->policyActionType == "1" //"POLICY_ACTION_TYPE_THROTTLE_DEVICE"
            ) {
                if (this->opts->throttlefrequencymin == -200000) {
                    (*json)["is_success"] = false;
                    (*json)["error"] = "Wrong argument: <throttlefrequencymin> should be specified by --throttlefrequencymin option";
                    return json;
                }
                if (this->opts->throttlefrequencymax == -200000) {
                    (*json)["is_success"] = false;
                    (*json)["error"] = "Wrong argument: <throttlefrequencymax> should be specified by --throttlefrequencymax option";
                    return json;
                }
                policy.mutable_action()->set_throttle_device_frequency_min(this->opts->throttlefrequencymin);
                policy.mutable_action()->set_throttle_device_frequency_max(this->opts->throttlefrequencymax);
            }
            policy.mutable_action()->set_type(this->policyActionTypeEnumFromString(this->opts->policyActionType));
        }

        //isTypeConditionActionMatch
        if (!this->isTypeConditionActionMatch()) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <type> <condition> <action> do not match. Please list matched items by --listalltypes option";
            return json;
        }

        //set_notifycallbackurl
        policy.set_notifycallbackurl("NoCallBackFromCli");
        //policy.set_notifycallbackurl("https://www.baidu.com/");

        //std::unique_ptr<nlohmann::json> CoreStub::setPolicy(bool isDevcie,int id,XpumPolicyData &policy) {
        json = this->coreStub->setPolicy(isDevcie, id, policy);
        return json;
    }
    if (this->opts->remove) {
        if (this->opts->deviceId == -1 && this->opts->groupId == -1) {
            // (*json)["error"] = "Too many operation flags";
            // return json;
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <device> or <group> should be specified by -d or -g option";
            return json;
        }

        //
        bool isDevcie = true;
        int id = this->opts->deviceId;
        if (this->opts->groupId != -1) {
            isDevcie = false;
            id = this->opts->groupId;
        }
        XpumPolicyData policy;
        policy.set_deviceid(id);
        if (this->opts->policyType.length() == 0) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <type> should be specified by --type option";
            return json;
        }
        if (!(this->opts->policyType == "1" || this->opts->policyType == "2" || this->opts->policyType == "3" || this->opts->policyType == "4" || this->opts->policyType == "5")) {
            (*json)["is_success"] = false;
            (*json)["error"] = "Wrong argument: <type> is invalid";
            return json;
        }
        policy.set_type(this->policyTypeEnumFromString(this->opts->policyType));
        policy.set_isdeletepolicy(true);
        //std::unique_ptr<nlohmann::json> CoreStub::setPolicy(bool isDevcie,int id,XpumPolicyData &policy) {
        json = this->coreStub->setPolicy(isDevcie, id, policy);
        return json;
    }
    return json;
}

bool ComletPolicy::isTypeConditionActionMatch() {
    // +-------------------------------+---------------+--------------------------------------------------+
    // | Types                         | Conditions    | Actions                                          |
    // +-------------------------------+---------------+--------------------------------------------------+
    // | 1. GPU Core Temperature       | 1. More than  | 1. Throttle GPU Core                             |
    // | 2. Programming Errors         | 1. More than  | 2. Reset GPU                                     |
    // | 3. Driver Errors              | 1. More than  | 2. Reset GPU                                     |
    // | 4. Cache Errors Correctable   | 1. More than  | 2. Reset GPU                                     |
    // | 5. Cache Errors Uncorrectable | 2. When occur | 2. Reset GPU                                     |
    // +-------------------------------+---------------+--------------------------------------------------+
    bool isMatch = false;
    if (!isMatch && (this->opts->policyType == "1" && this->opts->policyConditionType == "1" && this->opts->policyActionType == "1")) {
        isMatch = true;
    }
    if (!isMatch && (this->opts->policyType == "2" && this->opts->policyConditionType == "1" && this->opts->policyActionType == "2")) {
        isMatch = true;
    }
    if (!isMatch && (this->opts->policyType == "3" && this->opts->policyConditionType == "1" && this->opts->policyActionType == "2")) {
        isMatch = true;
    }
    if (!isMatch && (this->opts->policyType == "4" && this->opts->policyConditionType == "1" && this->opts->policyActionType == "2")) {
        isMatch = true;
    }
    if (!isMatch && (this->opts->policyType == "5" && this->opts->policyConditionType == "2" && this->opts->policyActionType == "2")) {
        isMatch = true;
    }
    return isMatch;
}

XpumPolicyType ComletPolicy::policyTypeEnumFromString(std::string &type) {
    // 1. GPU Core Temperature
    // 2. Programming Errors
    // 3. Driver Errors
    // 4. Cache Errors Correctable
    // 5. Cache Errors Uncorrectable
    if (type == "1") {
        return POLICY_TYPE_GPU_TEMPERATURE;
    } else if (type == "2") {
        return POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
    } else if (type == "3") {
        return POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS;
    } else if (type == "4") {
        return POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
    } else if (type == "5") {
        return POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
    } else {
        return POLICY_TYPE_GPU_TEMPERATURE;
    }

    // if (type == "POLICY_TYPE_GPU_TEMPERATURE") {
    //     return POLICY_TYPE_GPU_TEMPERATURE;
    // }else if (type == "POLICY_TYPE_GPU_MEMORY_TEMPERATURE") {
    //     return POLICY_TYPE_GPU_MEMORY_TEMPERATURE;
    // }else if (type == "POLICY_TYPE_GPU_POWER") {
    //     return POLICY_TYPE_GPU_POWER;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_RESET") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_RESET;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE;
    // }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE") {
    //     return POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE;
    // }else{
    //     return POLICY_TYPE_GPU_TEMPERATURE;
    // }
}

XpumPolicyConditionType ComletPolicy::policyConditionTypeEnumFromString(std::string &type) {
    // 1. More than
    // 2. When occur
    if (type == "2") {
        return POLICY_CONDITION_TYPE_WHEN_INCREASE;
    } else if (type == "1") {
        return POLICY_CONDITION_TYPE_GREATER;
    } else {
        return POLICY_CONDITION_TYPE_LESS;
    }
}

XpumPolicyActionType ComletPolicy::policyActionTypeEnumFromString(std::string &type) {
    // 1. Throttle GPU Core
    // 2. Reset GPU
    if (type == "1") {
        return POLICY_ACTION_TYPE_THROTTLE_DEVICE;
    }
    // else if (type == "2") {
    //     return POLICY_ACTION_TYPE_RESET_DEVICE;
    // }
    else {
        return POLICY_ACTION_TYPE_NULL;
    }
}

static void showAllSupported(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigAllSupported, *json);
    table.show(out);
}

static void showListDevice(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigListDevice, *json);
    table.show(out);
}

static void showListMulti(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigListAll, *json);
    table.show(out);
}

static void showCreateResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    out << (*json)["msg"].get<std::string>() << std::endl;
}

static void showRemoveResult(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    out << (*json)["msg"].get<std::string>() << std::endl;
}

void ComletPolicy::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (this->opts->listAll) {
        if (this->opts->deviceId >= 0) {
            showListDevice(out, json);
        } else if (this->opts->groupId > 0) {
            showListDevice(out, json);
        } else {
            showListMulti(out, json);
        }
        return;
    }
    if (this->opts->listalltypes) {
        showAllSupported(out, json);
        return;
    }

    if (this->opts->create) {
        showCreateResult(out, json);
        return;
    }
    if (this->opts->remove) {
        showRemoveResult(out, json);
        return;
    }
}

} // end namespace xpum::cli
