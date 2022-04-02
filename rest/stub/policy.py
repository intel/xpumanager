#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file policy.py
#

from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2
import datetime
import traceback
import json
import urllib
import xpum_logger as logger

XpumPolicyTypeToString = {
    core_pb2.POLICY_TYPE_GPU_TEMPERATURE: "XPUM_POLICY_TYPE_GPU_TEMPERATURE",
    core_pb2.POLICY_TYPE_GPU_MEMORY_TEMPERATURE: "XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE",
    core_pb2.POLICY_TYPE_GPU_POWER: "XPUM_POLICY_TYPE_GPU_POWER",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_RESET: "XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS: "XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS: "XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE: "XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE",
    core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE: "XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE",
    # core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE : "XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE",
    # core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE : "XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE",
    core_pb2.POLICY_TYPE_GPU_MISSING: "XPUM_POLICY_TYPE_GPU_MISSING",
    core_pb2.POLICY_TYPE_MAX: "XPUM_POLICY_TYPE_MAX",
}

XpumPolicyConditionTypeToString = {
    core_pb2.POLICY_CONDITION_TYPE_GREATER: "XPUM_POLICY_CONDITION_TYPE_GREATER",
    core_pb2.POLICY_CONDITION_TYPE_LESS: "XPUM_POLICY_CONDITION_TYPE_LESS",
    core_pb2.POLICY_CONDITION_TYPE_WHEN_INCREASE: "XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR",
}

XpumPolicyActionTypeToString = {
    core_pb2.POLICY_ACTION_TYPE_NULL: "XPUM_POLICY_ACTION_TYPE_NULL",
    core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE: "XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE",
    # core_pb2.POLICY_ACTION_TYPE_RESET_DEVICE : "XPUM_POLICY_ACTION_TYPE_RESET_DEVICE",
}

XpumPolicyTypeFromString = {
    "XPUM_POLICY_TYPE_GPU_TEMPERATURE": core_pb2.POLICY_TYPE_GPU_TEMPERATURE,
    "XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE": core_pb2.POLICY_TYPE_GPU_MEMORY_TEMPERATURE,
    "XPUM_POLICY_TYPE_GPU_POWER": core_pb2.POLICY_TYPE_GPU_POWER,
    "XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_RESET,
    "XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS,
    "XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS,
    "XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE,
    "XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE,
    # "XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE,
    # "XPUM_POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE": core_pb2.POLICY_TYPE_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE,
    "XPUM_POLICY_TYPE_GPU_MISSING": core_pb2.POLICY_TYPE_GPU_MISSING,
    "XPUM_POLICY_TYPE_MAX": core_pb2.POLICY_TYPE_MAX,
}

XpumPolicyConditionTypeFromString = {
    "XPUM_POLICY_CONDITION_TYPE_GREATER": core_pb2.POLICY_CONDITION_TYPE_GREATER,
    "XPUM_POLICY_CONDITION_TYPE_LESS": core_pb2.POLICY_CONDITION_TYPE_LESS,
    "XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR": core_pb2.POLICY_CONDITION_TYPE_WHEN_INCREASE,
}

XpumPolicyActionTypeFromString = {
    "XPUM_POLICY_ACTION_TYPE_NULL": core_pb2.POLICY_ACTION_TYPE_NULL,
    "XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE": core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE,
    # "XPUM_POLICY_ACTION_TYPE_RESET_DEVICE": core_pb2.POLICY_ACTION_TYPE_RESET_DEVICE,
}


def getPolicy(id, isDevcie):
    ##########
    try:
        resp = stub.getPolicy(
            core_pb2.GetPolicyRequest(id=id, isDevcie=isDevcie))
    except:
        traceback.print_exc()
        print("Failed to getPolicy:id={},isDevcie={}".format(id, isDevcie))
        return 2, 'gRPC Error When getPolicy', None, 500
    ##########
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None, 400
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
            action["throttle_device_frequency_min"] = one.action.throttle_device_frequency_min
            action["throttle_device_frequency_max"] = one.action.throttle_device_frequency_max
        data['action'] = action
        ####
        data['notify_callback_url'] = one.notifyCallBackUrl
        ret.append(data)
    return 0, "OK", ret, 200


def isValidNotifyCallbackUrl(notify_callback_url):
    if notify_callback_url is None or len(notify_callback_url) == 0:
        return False
    if not (str(notify_callback_url).startswith('http://') or str(notify_callback_url).startswith('https://')):
        return False
    return True


def setPolicy(id, isDevcie, input, is_delete_policy):
    ##########
    if "type" not in input or len(input["type"]) == 0 or input["type"] not in XpumPolicyTypeFromString:
        return 1, "Invalid Parameter: policy type is invalid.", 400
    policyType = XpumPolicyTypeFromString[input["type"]]
    ##########
    actionTag = "set"
    if is_delete_policy:
        actionTag = "remove"
        policy = core_pb2.XpumPolicyData(
            type=policyType, isDeletePolicy=is_delete_policy)
    else:
        ##########
        if "condition" not in input:
            return 1, "Invalid Parameter: policy condition is invalid.", 400
        if "type" not in input["condition"] or input["condition"]["type"] not in XpumPolicyConditionTypeFromString:
            return 1, "Invalid Parameter: policy condition type is invalid.", 400
        policyConditionType = XpumPolicyConditionTypeFromString[input["condition"]["type"]]
        if(policyConditionType == core_pb2.POLICY_CONDITION_TYPE_LESS or policyConditionType == core_pb2.POLICY_CONDITION_TYPE_GREATER):
            if "threshold" not in input["condition"]:
                return 1, "Invalid Parameter: policy condition threshold is invalid.", 400
            if int(input["condition"]["threshold"]) < 0:
                return 1, "Invalid Parameter: policy condition threshold must be greater than or equal 0.", 400
            condition = core_pb2.XpumPolicyCondition(
                type=policyConditionType, threshold=input["condition"]["threshold"])
        else:
            condition = core_pb2.XpumPolicyCondition(type=policyConditionType)
        ##########
        if "action" not in input:
            return 1, "Invalid Parameter: policy action is invalid.", 400
        if "type" not in input["action"] or input["action"]["type"] not in XpumPolicyActionTypeFromString:
            return 1, "Invalid Parameter: policy action type is invalid.", 400
        policyActionType = XpumPolicyActionTypeFromString[input["action"]["type"]]
        if(policyActionType == core_pb2.POLICY_ACTION_TYPE_THROTTLE_DEVICE):
            if "throttle_device_frequency_min" not in input["action"]:
                return 1, "Invalid Parameter: policy action throttle_device_frequency_min is invalid.", 400
            if "throttle_device_frequency_max" not in input["action"]:
                return 1, "Invalid Parameter: policy action throttle_device_frequency_max is invalid.", 400
            action = core_pb2.XpumPolicyAction(type=policyActionType, throttle_device_frequency_min=input["action"][
                                               "throttle_device_frequency_min"], throttle_device_frequency_max=input["action"]["throttle_device_frequency_max"])
        elif(policyActionType == core_pb2.POLICY_ACTION_TYPE_NULL):
            if 'notify_callback_url' not in input:
                return 1, "Invalid Parameter: policy notify_callback_url must be filled when no other actions.", 400
            action = core_pb2.XpumPolicyAction(type=policyActionType)
        else:
            action = core_pb2.XpumPolicyAction(type=policyActionType)
        ##########
        if 'notify_callback_url' in input:
            notify_callback_url = input['notify_callback_url']
            if not isValidNotifyCallbackUrl(notify_callback_url):
                return 1, "Invalid Parameter: policy notify_callback_url must be valid Url with http:// or https:// schema.", 400
        else:
            notify_callback_url = "NoCallBackFromRest"
        policy = core_pb2.XpumPolicyData(
            type=policyType, condition=condition, action=action, notifyCallBackUrl=notify_callback_url)
    ##########
    resp = stub.setPolicy(core_pb2.SetPolicyRequest(
        id=id, isDevcie=isDevcie, policy=policy))
    ##########
    if len(resp.errorMsg) != 0:
        logger.audit("{}Policy".format(actionTag), 'Failed',
                     "Failed to {} the {} policy. Error message: {}", actionTag, input["type"], resp.errorMsg)
        return 2, resp.errorMsg, 500
    else:
        logger.audit("{}Policy".format(actionTag), 'Success',
                     "Succeed to {} the {} policy. ", actionTag, input["type"])
        return 0, "OK", 200


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
            notify_callback_url = one.notifyCallBackUrl
            if notify_callback_url is None or len(notify_callback_url) == 0 or str(notify_callback_url).startswith('NoCallBackFrom'):
                print("{} - CallBackPolicyData: Ignored to send because notify_callback_url is invalid:{} -- policyType={}; policyTimestamp={};".format(
                    time_stamp, one.notifyCallBackUrl, data['type'], one.timestamp,))
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
            url = notify_callback_url+"?data="+urllib.parse.quote(json_str)
            if not isValidNotifyCallbackUrl(url):
                print("{} - CallBackPolicyData: Failed to send callback url:{} -- policyType={}; policyTimestamp={}; Because callback url is invalid.".format(
                    time_stamp, one.notifyCallBackUrl, data['type'], one.timestamp))
                continue
            response = urllib.request.urlopen(url)
            html = response.read()
            # print(html)
            print("{} - CallBackPolicyData: Success to send callback url:{} -- policyType={}; policyTimestamp={}; ".format(
                time_stamp, one.notifyCallBackUrl, data['type'], one.timestamp))
            pass
        except:
            traceback.print_exc()
            print("{} - CallBackPolicyData: Failed to send callback url:{} -- policyType={}; policyTimestamp={}; ".format(
                time_stamp, one.notifyCallBackUrl, data['type'], one.timestamp))
