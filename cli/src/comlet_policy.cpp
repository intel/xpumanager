#include "comlet_policy.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {

void ComletPolicy::setupOptions() {
    this->opts = std::unique_ptr<ComletPolicyOptions>(new ComletPolicyOptions());
    addFlag("-l,--list", this->opts->listAll, "list all policies");
    addFlag("--listAllPolicyType", this->opts->listAllPolicyType, "list all policy types and its supported condition type and action type");
    addFlag("-c,--create", this->opts->create, "create policy");
    addFlag("-r,--remove", this->opts->remove, "remove policy"); 
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-g,--group", this->opts->groupId, "group id");
    addOption("--policyType", this->opts->policyType, "policy type");
    addOption("--policyConditionType", this->opts->policyConditionType, "policy condition type");
    addOption("--policyActionType", this->opts->policyActionType, "policy action type");
    addOption("-t,--threshold", this->opts->threshold, "threshold");
    addOption("--throttleDeviceFrequencyMin", this->opts->throttleDeviceFrequencyMin, "throttle device frequency min");
    addOption("--throttleDeviceFrequencyMax", this->opts->throttleDeviceFrequencyMax, "throttle device frequency max");
}

std::unique_ptr<nlohmann::json> ComletPolicy::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->listAll) {
        json = this->coreStub->getAllPolicy();
        return json;
    } 
    if (this->opts->listAllPolicyType) {
        json = this->coreStub->getAllPolicyType();
        return json;
    }
    if (this->opts->create) {
        if (this->opts->deviceId == -1 && this->opts->groupId == -1) {
            // (*json)["error"] = "Too many operation flags";
            // return json;
            (*json)["error"] = "Wrong argument: <deviceId> or <groupId> should be specified by -d or -g option";
            return json;
        } 
        
        //
        bool isDevcie = true;
        int id = this->opts->deviceId;
        if (this->opts->groupId != -1){
            isDevcie = false;
            id = this->opts->groupId;
        }         
        XpumPolicyData policy;  
        policy.set_deviceid(id);
        if (this->opts->policyType.length() == 0) {
            (*json)["error"] = "Wrong argument: <policyType> should be specified by --policyType option";
            return json;
        } 
        policy.set_type(this->policyTypeEnumFromString(this->opts->policyType));

        //
        if (this->opts->policyConditionType.length() == 0) {
            (*json)["error"] = "Wrong argument: <policyConditionType> should be specified by --policyConditionType option";
            return json;
        } else {   
            if(this->opts->policyConditionType == "POLICY_CONDITION_TYPE_GREATER"
                || this->opts->policyConditionType == "POLICY_CONDITION_TYPE_LESS"){
                if (this->opts->threshold == -2) {
                    (*json)["error"] = "Wrong argument: <threshold> should be specified by -t option";
                    return json;
                }
                policy.mutable_condition()->set_threshold(this->opts->threshold);
            }
            policy.mutable_condition()->set_type(this->policyConditionTypeEnumFromString(this->opts->policyConditionType));
        }
        if (this->opts->policyActionType.length() == 0) {
            (*json)["error"] = "Wrong argument: <policyActionType> should be specified by --policyActionType option";
            return json;
        } else {
            if(this->opts->policyActionType == "POLICY_ACTION_TYPE_THROTTLE_DEVICE"){
                if (this->opts->throttleDeviceFrequencyMin == -200000) {
                    (*json)["error"] = "Wrong argument: <throttleDeviceFrequencyMin> should be specified by --throttleDeviceFrequencyMin option";
                    return json;
                }
                if (this->opts->throttleDeviceFrequencyMax == -200000) {
                    (*json)["error"] = "Wrong argument: <throttleDeviceFrequencyMax> should be specified by --throttleDeviceFrequencyMax option";
                    return json;
                }
                policy.mutable_action()->set_throttle_device_frequency_min(this->opts->throttleDeviceFrequencyMin);
                policy.mutable_action()->set_throttle_device_frequency_max(this->opts->throttleDeviceFrequencyMax);
            }
            policy.mutable_action()->set_type(this->policyActionTypeEnumFromString(this->opts->policyActionType));
        }

        //set_notifycallbackurl
        policy.set_notifycallbackurl("NoCallBackFromCli");
        //policy.set_notifycallbackurl("https://www.baidu.com/");

        //std::unique_ptr<nlohmann::json> CoreStub::setPolicy(bool isDevcie,int id,XpumPolicyData &policy) {
        json = this->coreStub->setPolicy(isDevcie,id,policy);
        return json;
    }
    if (this->opts->remove) {
        if (this->opts->deviceId == -1 && this->opts->groupId == -1) {
            // (*json)["error"] = "Too many operation flags";
            // return json;
            (*json)["error"] = "Wrong argument: <deviceId> or <groupId> should be specified by -d or -g option";
            return json;
        } 
        
        //
        bool isDevcie = true;
        int id = this->opts->deviceId;
        if (this->opts->groupId != -1){
            isDevcie = false;
            id = this->opts->groupId;
        }         
        XpumPolicyData policy;  
        policy.set_deviceid(id);
        if (this->opts->policyType.length() == 0) {
            (*json)["error"] = "Wrong argument: <policyType> should be specified by --policyType option";
            return json;
        } 
        policy.set_type(this->policyTypeEnumFromString(this->opts->policyType));
        policy.set_isdeletepolicy(true);
        //std::unique_ptr<nlohmann::json> CoreStub::setPolicy(bool isDevcie,int id,XpumPolicyData &policy) {
        json = this->coreStub->setPolicy(isDevcie,id,policy);
        return json;
    }
    return json;
}

XpumPolicyType ComletPolicy::policyTypeEnumFromString(std::string &type) {
    if (type == "POLICY_TYPE_GPU_TEMPERATURE") {
        return POLICY_TYPE_GPU_TEMPERATURE;
    }else if (type == "POLICY_TYPE_GPU_MEMORY_TEMPERATURE") {
        return POLICY_TYPE_GPU_MEMORY_TEMPERATURE;
    }else if (type == "POLICY_TYPE_GPU_POWER") {
        return POLICY_TYPE_GPU_POWER;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_RESET") {
        return POLICY_TYPE_RAS_ERROR_CAT_RESET;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS") {
        return POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS") {
        return POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE") {
        return POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE") {
        return POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE") {
        return POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE;
    }else if (type == "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE") {
        return POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE;
    }else{
        return POLICY_TYPE_GPU_TEMPERATURE;
    }
}


XpumPolicyConditionType ComletPolicy::policyConditionTypeEnumFromString(std::string& type) {
    if (type == "POLICY_CONDITION_TYPE_WHEN_INCREASE") {
        return POLICY_CONDITION_TYPE_WHEN_INCREASE;
    }else if (type == "POLICY_CONDITION_TYPE_LESS") {
        return POLICY_CONDITION_TYPE_LESS;
    }else{
        return POLICY_CONDITION_TYPE_GREATER;
    }
}

XpumPolicyActionType ComletPolicy::policyActionTypeEnumFromString(std::string& type) {
    if (type == "POLICY_ACTION_TYPE_THROTTLE_DEVICE") {
        return POLICY_ACTION_TYPE_THROTTLE_DEVICE;
    }else if (type == "POLICY_ACTION_TYPE_RESET_DEVICE") {
        return POLICY_ACTION_TYPE_RESET_DEVICE;
    }else{
        return POLICY_ACTION_TYPE_NULL;
    }
}

} // end namespace xpum::cli
