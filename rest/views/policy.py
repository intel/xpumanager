#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file policy.py
#

import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields
from marshmallow.base import FieldABC
import time
import threading
import datetime


class PolicyConditionSchema(Schema):
    type = fields.Str(metadata={
                      "description": "Policy conditon type. Supported types: XPUM_POLICY_CONDITION_TYPE_GREATER, XPUM_POLICY_CONDITION_TYPE_LESS, XPUM_POLICY_CONDITION_TYPE_WHEN_OCCUR"})
    threshold = fields.Int(
        metadata={"description": "The threshold for policy"})


class PolicyActionSchema(Schema):
    type = fields.Str(metadata={
                      "description": "Policy action type. Supported types: XPUM_POLICY_ACTION_TYPE_THROTTLE_DEVICE, XPUM_POLICY_ACTION_TYPE_NULL"})
    throttle_device_frequency_min = fields.Int(metadata={
                                               "description": "The throttle_device_frequency_min value only for POLICY_ACTION_TYPE_THROTTLE_DEVICE action type."})
    throttle_device_frequency_max = fields.Int(metadata={
                                               "description": "The throttle_device_frequency_max value only for POLICY_ACTION_TYPE_THROTTLE_DEVICE action type."})


class PolicySchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    type = fields.Str(metadata={"description": "Policy type. Supported types: XPUM_POLICY_TYPE_GPU_TEMPERATURE, XPUM_POLICY_TYPE_GPU_MEMORY_TEMPERATURE, XPUM_POLICY_TYPE_GPU_POWER, XPUM_POLICY_TYPE_RAS_ERROR_CAT_RESET, XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS, XPUM_POLICY_TYPE_RAS_ERROR_CAT_DRIVER_ERRORS, XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, XPUM_POLICY_TYPE_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, XPUM_POLICY_TYPE_GPU_MISSING, XPUM_POLICY_TYPE_GPU_THROTTLE"})
    notify_callback_url = fields.Str(
        metadata={"description": "Policy notify callback url"})
    action = fields.Nested(PolicyActionSchema)
    condition = fields.Nested(PolicyConditionSchema)
    #is_delete_policy = fields.Bool(metadata={"description": "is_delete_policy flag when remove policy"})


class PolicyGetSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    policy_list = fields.Nested(PolicySchema, many=True)


class PolicyRespSchema(Schema):
    status = fields.Int(
        metadata={"description": "status code, 0 is success, other is error."})
    message = fields.Str(metadata={"description": "success or error message"})


def get_device_policy(deviceId):
    """
    Get all policies for a device
    ---
    get:
        tags:
            - "Policy"
        description: Get all policies for a device
        parameters:
            - 
                name: deviceId
                in: path
                description: Device id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: 
                    type: array
                    items: PolicyGetSchema
                examples: 
                    application/json:
                        [ { "device_id": 0, "policy_list": [ { "action": { "type": "XPUM_POLICY_ACTION_TYPE_NULL" }, "condition": { "threshold": 11, "type": "XPUM_POLICY_CONDITION_TYPE_LESS" }, "device_id": 0, "notify_callback_url": "http://abc.test.com:6443", "type": "XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS" } ] } ]
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "There is no data", "status": 1 }
    """
    id = deviceId
    ret = []
    code, message, data, httpCode = stub.getPolicy(id, True)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), httpCode
    retOne = {}
    retOne["device_id"] = id
    retOne["policy_list"] = data
    ret.append(retOne)
    return jsonify(ret), 200


def set_device_policy(deviceId):
    """
    Set a policy for a device
    ---
    post:
        tags:
            - "Policy"
        description: Set a policy for a device. 
        parameters:
            - 
                name: policy info
                in: body
                description: policy info which will be set into a device
                schema: PolicySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "success", "status": 0 }
            400:
                description: Request Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Invalid Parameter: policy type is invalid.", "status": 1 }
            500:
                description: Internal Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Internal Error", "status": 2 }
    delete:
        tags:
            - "Policy"
        description: Delete a policy for a device. The policy type must be set.
        parameters:
            - 
                name: policy info
                in: body
                description: policy info which will be deleted from a device. the policy type must be set.
                schema: PolicySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "success", "status": 0 }
            400:
                description: Request Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Invalid Parameter: policy type is invalid.", "status": 1 }
            500:
                description: Internal Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Internal Error", "status": 2 }
    """
    id = deviceId
    policy = request.get_json()
    if request.method == 'DELETE':
        is_delete_policy = True
    else:
        is_delete_policy = False
    #######
    code, message, httpCode = stub.setPolicy(
        id, True, policy, is_delete_policy)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), httpCode
    #######
    ret = dict(status=0, message="success")
    return jsonify(ret), 200


def get_group_policy(groupId):
    """
    Get all policies for a group
    ---
    get:
        tags:
            - "Policy"
        description: Get all policies for a group
        parameters:
            - 
                name: groupId
                in: path
                description: Group id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: 
                    type: array
                    items: PolicyGetSchema
                examples: 
                    application/json:
                        [ { "device_id": 0, "policy_list": [ { "action": { "type": "XPUM_POLICY_ACTION_TYPE_NULL" }, "condition": { "threshold": 11, "type": "XPUM_POLICY_CONDITION_TYPE_LESS" }, "device_id": 0, "notify_callback_url": "http://abc.test.com:6443", "type": "XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS" } ] } ]
            400:
                description: Request Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "There is no data", "status": 1 }
            500:
                description: Internal Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Internal Error", "status": 2 }
    """
    id = groupId
    ret = []
    code, message, data, httpCode = stub.getPolicy(id, False)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), httpCode
    retOne = {}
    retOne["group_id"] = id
    retOne["policy_list"] = data
    ret.append(retOne)
    return jsonify(ret), 200


def set_group_policy(groupId):
    """
    Set a policy for a group
    ---
    post:
        tags:
            - "Policy"
        description: Set a policy for a group. 
        parameters:
            - 
                name: policy info
                in: body
                description: policy info which will be set into a group
                schema: PolicySchema
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "success", "status": 0 }
            400:
                description: Request Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Invalid Parameter: policy type is invalid.", "status": 1 }
            500:
                description: Internal Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Internal Error", "status": 2 }
    delete:
        tags:
            - "Policy"
        description: Delete a policy for a group. The policy type must be set.
        parameters:
            - 
                name: policy info
                in: body
                description: policy info which will be deleted from a group. the policy type must be set.
                schema: PolicySchema
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "success", "status": 0 }
            400:
                description: Request Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Invalid Parameter: policy type is invalid.", "status": 1 }
            500:
                description: Internal Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Internal Error", "status": 2 }
    """
    id = groupId
    policy = request.get_json()
    if request.method == 'DELETE':
        is_delete_policy = True
    else:
        is_delete_policy = False
    #######
    code, message, httpCode = stub.setPolicy(
        id, False, policy, is_delete_policy)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), httpCode
    #######
    ret = dict(status=0, message="success")
    return jsonify(ret), 200


def get_all_policy():
    """
    Get all policies for all devices
    ---
    get:
        tags:
            - "Policy"
        description: Get all policies for all devices
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: 
                    type: array
                    items: PolicyGetSchema
                examples: 
                    application/json:
                        [ { "device_id": 0, "policy_list": [ { "action": { "type": "XPUM_POLICY_ACTION_TYPE_NULL" }, "condition": { "threshold": 11, "type": "XPUM_POLICY_CONDITION_TYPE_LESS" }, "device_id": 0, "notify_callback_url": "http://abc.test.com:6443", "type": "XPUM_POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS" } ] } ]
            400:
                description: Request Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "There is no data", "status": 1 }
            500:
                description: Internal Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "message": "Internal Error", "status": 2 }
    """
    code, message, data = stub.getDeviceList()
    if code != 0:
        error = dict(status=3, message=message)
        return jsonify(error), 400
    #####
    ret = []
    #####
    for one in data:
        id = one["device_id"]
        code, message, data, httpCode = stub.getPolicy(id, True)
        if code != 0:
            # error = dict(status=code, message=message)
            # return jsonify(error), httpCode
            continue
        retOne = {}
        retOne["device_id"] = id
        retOne["policy_list"] = data
        ret.append(retOne)
    #####
    if len(ret) == 0:
        code, message, data, httpCode = 1, "There is no data", None, 400
        error = dict(status=code, message=message)
        return jsonify(error), httpCode
    else:
        return jsonify(ret), 200


def policyCallBackThread():
    while True:
        try:
            stub.readPolicyNotifyData()
        except:
            time.sleep(5)
            # traceback.print_exc()
            time_stamp = datetime.datetime.now()
            time_stamp = time_stamp.strftime('[%Y-%m-%d %H:%M:%S]')
            print("{}: policyCallBackThread(): Failed to readPolicyNotifyData!".format(
                time_stamp))


def startPolicyCallBackThread():
    t = threading.Thread(target=policyCallBackThread, name='policyCallBackRun')
    t.daemon = True
    t.start()
    pass
