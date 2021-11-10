from .grpc_stub import stub
import core_pb2

XpumPolicyTypeToString = {
    core_pb2.POLICY_TYPE_GPU_TEMPERATURE : "POLICY_TYPE_GPU_TEMPERATURE",
    core_pb2.POLICY_TYPE_GPU_MEMORY_TEMPERATURE : "POLICY_TYPE_GPU_MEMORY_TEMPERATURE",
    core_pb2.POLICY_TYPE_GPU_POWER : "POLICY_TYPE_GPU_POWER",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_RESET : "POLICY_TYPE_RAS_ERROR_CAT_RESET",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS : "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS : "POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE : "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE : "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE : "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE : "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE",
    core_pb2.POLICY_TYPE_MAX : "POLICY_TYPE_MAX",
}

XpumPolicyConditionTypeToString = {  
    core_pb2.POLICY_CONDITION_TYPE_GREATER : "POLICY_CONDITION_TYPE_GREATER",
    core_pb2.POLICY_CONDITION_TYPE_LESS : "POLICY_CONDITION_TYPE_LESS",
    core_pb2.POLICY_CONDITION_TYPE_WHEN_INCREASE : "POLICY_CONDITION_TYPE_WHEN_INCREASE",
}

XpumPolicyActionTypeToString = {  
    core_pb2.POLICY_ACTION_TYPE_NULL : "POLICY_ACTION_TYPE_NULL",
    core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE : "POLICY_ACTION_TYPE_THROTTLE_DEVICE",
    core_pb2.POLICY_ACTION_TYPE_RESET_DEVICE : "POLICY_ACTION_TYPE_RESET_DEVICE",   
}

XpumPolicyTypeFromString = {
    "POLICY_TYPE_GPU_TEMPERATURE": core_pb2.POLICY_TYPE_GPU_TEMPERATURE,
    "POLICY_TYPE_GPU_MEMORY_TEMPERATURE": core_pb2.POLICY_TYPE_GPU_MEMORY_TEMPERATURE,
    "POLICY_TYPE_GPU_POWER": core_pb2.POLICY_TYPE_GPU_POWER,
    "POLICY_TYPE_RAS_ERROR_CAT_RESET": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_RESET,
    "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
    "POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS,
    "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
    "POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
    "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
    "POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
    "POLICY_TYPE_MAX": core_pb2.POLICY_TYPE_MAX,
}

XpumPolicyConditionTypeFromString = {  
    "POLICY_CONDITION_TYPE_GREATER": core_pb2.POLICY_CONDITION_TYPE_GREATER,
    "POLICY_CONDITION_TYPE_LESS": core_pb2.POLICY_CONDITION_TYPE_LESS,
    "POLICY_CONDITION_TYPE_WHEN_INCREASE": core_pb2.POLICY_CONDITION_TYPE_WHEN_INCREASE,
}

XpumPolicyActionTypeFromString = {  
    "POLICY_ACTION_TYPE_NULL": core_pb2.POLICY_ACTION_TYPE_NULL,
    "POLICY_ACTION_TYPE_THROTTLE_DEVICE": core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE,
    "POLICY_ACTION_TYPE_RESET_DEVICE": core_pb2.POLICY_ACTION_TYPE_RESET_DEVICE,   
}


def getPolicy(id, isDevcie):  
    ##########
    resp = stub.getPolicy(core_pb2.GetPolicyRequest(id=id,isDevcie=isDevcie))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = []
    for d in resp.policyList:
        device = dict()
        device['device_id'] = d.deviceId
        device['type'] = XpumPolicyTypeToString[d.type]
        ####
        condition = {}
        condition["type"] = XpumPolicyConditionTypeToString[d.condition.type]
        if(d.condition.type == core_pb2.POLICY_CONDITION_TYPE_LESS or d.condition.type == core_pb2.POLICY_CONDITION_TYPE_GREATER):
            condition["threshold"] = d.condition.threshold
        device['condition'] = condition
        ####
        action = {}
        action["type"] = XpumPolicyActionTypeToString[d.action.type]
        if(d.action.type == core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE):
            condition["throttle_device_frequency_min"] = d.action.throttle_device_frequency_min
            condition["throttle_device_frequency_max"] = d.action.throttle_device_frequency_max
        device['action'] = action
        ####
        data.append(device)
    return 0, "OK", data

def setPolicy(id, isDevcie,input,isDeletePolicy): 
    ##########
    policyType = XpumPolicyTypeFromString[input["type"]]
    ##########
    if isDeletePolicy:
        policy = core_pb2.XpumPolicyData(type=policyType,isDeletePolicy=isDeletePolicy)
    else:
        policyConditionType = XpumPolicyConditionTypeFromString[input["condition"]["type"]]
        if(policyConditionType == core_pb2.POLICY_CONDITION_TYPE_LESS or policyConditionType == core_pb2.POLICY_CONDITION_TYPE_GREATER):
            condition = core_pb2.XpumPolicyCondition(type=policyConditionType,threshold=input["condition"]["threshold"])
        else:
            condition = core_pb2.XpumPolicyCondition(type=policyConditionType)
        ##########
        policyActionType = XpumPolicyActionTypeFromString[input["action"]["type"]]
        if(policyActionType == core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE):
            action = core_pb2.XpumPolicyAction(type=policyActionType
                ,throttle_device_frequency_min=input["action"]["throttle_device_frequency_min"]
                ,throttle_device_frequency_max=input["action"]["throttle_device_frequency_max"])
        else:
            action = core_pb2.XpumPolicyAction(type=policyActionType)
        ##########
        policy = core_pb2.XpumPolicyData(type=policyType,condition=condition,action=action)
    ##########
    resp = stub.setPolicy(core_pb2.SetPolicyRequest(id=id,isDevcie=isDevcie,policy=policy))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result":"success"}

