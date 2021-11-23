from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2
import datetime
import traceback
import json
import urllib

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
    try:
        resp = stub.getPolicy(core_pb2.GetPolicyRequest(id=id,isDevcie=isDevcie))
    except:
        traceback.print_exc()  
        print("Failed to getPolicy:id={},isDevcie={}".format(id,isDevcie))
        return 2, 'gRPC Error When getPolicy', None
    ##########    
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    ret = []
    for one in resp.policyList:
        data = dict()
        data['device_id'] = one.deviceId
        data['type'] = XpumPolicyTypeToString[one.type]
        ####
        condition = {}
        condition["type"] = XpumPolicyConditionTypeToString[one.condition.type]
        if(one.condition.type == core_pb2.POLICY_CONDITION_TYPE_LESS or one.condition.type == core_pb2.POLICY_CONDITION_TYPE_GREATER):
            condition["threshold"] = one.condition.threshold
        data['condition'] = condition
        ####
        action = {}
        action["type"] = XpumPolicyActionTypeToString[one.action.type]
        if(one.action.type == core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE):
            condition["throttle_device_frequency_min"] = one.action.throttle_device_frequency_min
            condition["throttle_device_frequency_max"] = one.action.throttle_device_frequency_max
        data['action'] = action
        ####
        data['notifyCallBackUrl'] = one.notifyCallBackUrl
        ret.append(data)
    return 0, "OK", ret

def setPolicy(id, isDevcie,input,isDeletePolicy): 
    ##########
    if "type" not in input or len(input["type"]) == 0 or input["type"] not in XpumPolicyTypeFromString:
        return 1, "Invalid Parameter: policy type is invalid.", {"result":"failed"}
    policyType = XpumPolicyTypeFromString[input["type"]]
    ##########
    if isDeletePolicy:
        policy = core_pb2.XpumPolicyData(type=policyType,isDeletePolicy=isDeletePolicy)
    else:
        ##########
        if "condition" not in input:
            return 1, "Invalid Parameter: policy condition is invalid.", {"result":"failed"}
        if "type" not in input["condition"] or input["condition"]["type"] not in XpumPolicyConditionTypeFromString:
            return 1, "Invalid Parameter: policy condition type is invalid.", {"result":"failed"}
        policyConditionType = XpumPolicyConditionTypeFromString[input["condition"]["type"]]
        if(policyConditionType == core_pb2.POLICY_CONDITION_TYPE_LESS or policyConditionType == core_pb2.POLICY_CONDITION_TYPE_GREATER):
            if "threshold" not in input["condition"]:
                return 1, "Invalid Parameter: policy condition threshold is invalid.", {"result":"failed"}            
            condition = core_pb2.XpumPolicyCondition(type=policyConditionType,threshold=input["condition"]["threshold"])
        else:
            condition = core_pb2.XpumPolicyCondition(type=policyConditionType)
        ##########
        if "action" not in input:
            return 1, "Invalid Parameter: policy action is invalid.", {"result":"failed"}
        if "type" not in input["action"] or input["action"]["type"] not in XpumPolicyActionTypeFromString:
            return 1, "Invalid Parameter: policy action type is invalid.", {"result":"failed"}
        policyActionType = XpumPolicyActionTypeFromString[input["action"]["type"]]
        if(policyActionType == core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE):
            if "throttle_device_frequency_min" not in input["action"]:
                return 1, "Invalid Parameter: policy action throttle_device_frequency_min is invalid.", {"result":"failed"}     
            if "throttle_device_frequency_max" not in input["action"]:
                return 1, "Invalid Parameter: policy action throttle_device_frequency_max is invalid.", {"result":"failed"}     
            action = core_pb2.XpumPolicyAction(type=policyActionType
                ,throttle_device_frequency_min=input["action"]["throttle_device_frequency_min"]
                ,throttle_device_frequency_max=input["action"]["throttle_device_frequency_max"])
        else:
            action = core_pb2.XpumPolicyAction(type=policyActionType)
        ##########
        if 'notifyCallBackUrl' in input:
            notifyCallBackUrl = input['notifyCallBackUrl']            
        else:
            notifyCallBackUrl = "NoCallBackFromRest"
        policy = core_pb2.XpumPolicyData(type=policyType,condition=condition,action=action,notifyCallBackUrl=notifyCallBackUrl)
    ##########
    resp = stub.setPolicy(core_pb2.SetPolicyRequest(id=id,isDevcie=isDevcie,policy=policy))
    if len(resp.errorMsg) != 0:
        return 2, resp.errorMsg, None
    return 0, "OK", {"result":"success"}

def readPolicyNotifyData(): 
    notifys = stub.readPolicyNotifyData(empty_pb2.Empty())
    for one in notifys:
        time_stamp = datetime.datetime.now()
        time_stamp = time_stamp.strftime('[%Y-%m-%d %H:%M:%S]')
        try:
            ####
            data = dict()
            data['device_id'] = one.deviceId
            data['type'] = XpumPolicyTypeToString[one.type]
            ####
            notifyCallBackUrl = one.notifyCallBackUrl
            if notifyCallBackUrl is None or len(notifyCallBackUrl) == 0 or str(notifyCallBackUrl).startswith('NoCallBackFrom'):
                print("{} - CallBackPolicyData: Ignored to send because notifyCallBackUrl is invalid:{} -- policyType={}; policyTimestamp={};".format(time_stamp,one.notifyCallBackUrl,data['type'], one.timestamp,))
                continue
            ####
            condition = {}
            condition["type"] = XpumPolicyConditionTypeToString[one.condition.type]
            if(one.condition.type == core_pb2.POLICY_CONDITION_TYPE_LESS or one.condition.type == core_pb2.POLICY_CONDITION_TYPE_GREATER):
                condition["threshold"] = one.condition.threshold
            data['condition'] = condition
            ####
            action = {}
            action["type"] = XpumPolicyActionTypeToString[one.action.type]
            if(one.action.type == core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE):
                condition["throttle_device_frequency_min"] = one.action.throttle_device_frequency_min
                condition["throttle_device_frequency_max"] = one.action.throttle_device_frequency_max
            data['action'] = action
            ####
            data['timestamp'] = one.timestamp
            data['curValue'] = one.curValue
            data['isTileData'] = one.isTileData
            data['tileId'] = one.tileId
            ####
            json_str = json.dumps(data)
            url = notifyCallBackUrl+"?data="+urllib.parse.quote(json_str)
            response = urllib.request.urlopen(url)
            html = response.read()
            #print(html)
            print("{} - CallBackPolicyData: Success to send callback url:{} -- policyType={}; policyTimestamp={}; ".format(time_stamp,one.notifyCallBackUrl,data['type'], one.timestamp))
            pass
        except:
            traceback.print_exc()  
            print("{} - CallBackPolicyData: Failed to send callback url:{} -- policyType={}; policyTimestamp={}; ".format(time_stamp,one.notifyCallBackUrl,data['type'], one.timestamp))
            
            
