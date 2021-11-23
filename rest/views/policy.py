import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields
from marshmallow.base import FieldABC
import time, threading
import datetime

class PolicyConditionSchema(Schema):
    type = fields.Str(metadata={"description": "Policy conditon type"})
    threshold = fields.Int(metadata={"description": "The threshold for policy"})

class PolicyActionSchema(Schema):
    type = fields.Str(metadata={"description": "Policy conditon type"})
    throttle_device_frequency_min = fields.Int(metadata={"description": "The throttle_device_frequency_min value only for POLICY_ACTION_TYPE_THROTTLE_DEVICE action type."})
    throttle_device_frequency_max = fields.Int(metadata={"description": "The throttle_device_frequency_max value only for POLICY_ACTION_TYPE_THROTTLE_DEVICE action type."})


class PolicySchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    type = fields.Str(metadata={"description": "Policy type"})
    notifyCallBackUrl = fields.Str(metadata={"description": "Policy notify callback url"})
    action = fields.Nested(PolicyActionSchema)
    condition = fields.Nested(PolicyConditionSchema)
    isDeletePolicy = fields.Bool(metadata={"description": "isDeletePolicy flag when remove policy"})

class PolicyGetSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    poliyList = fields.Nested(PolicySchema,many=True)

class PolicyRespSchema(Schema):  
    Status = fields.Int(metadata={"description": "Status code, 0 is success, other is error."})
    Message = fields.Str(metadata={"description": "success or error message"}) 

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
                        [ { "device_id": 0, "poliyList": [ { "action": { "type": "POLICY_ACTION_TYPE_NULL" }, "condition": { "threshold": 11, "type": "POLICY_CONDITION_TYPE_LESS" }, "device_id": 0, "notifyCallBackUrl": "http://abc.test.com:6443", "type": "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS" } ] } ]
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "There is no data", "Status": 1 }
    """  
    id = deviceId
    ret = []
    code, message, data = stub.getPolicy(id, True)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    retOne = {}
    retOne["device_id"] = id
    retOne["poliyList"] = data
    ret.append(retOne)
    return jsonify(ret)

def set_device_policy(deviceId):
    """
    Set a policy for a device
    ---
    post:
        tags:
            - "Policy"
        description: Set a policy for a device. The isDeletePolicy must be false.
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
                        { "Message": "success", "Status": 0 }
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "Invalid Parameter: policy type is invalid.", "Status": 1 }
    delete:
        tags:
            - "Policy"
        description: Delete a policy for a device. The policy type must be set, isDeletePolicy must be true.
        parameters:
            - 
                name: policy info
                in: body
                description: policy info which will be deleted from a device. the policy type must be set, isDeletePolicy must be true.
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
                        { "Message": "success", "Status": 0 }
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "Invalid Parameter: policy type is invalid.", "Status": 1 }
    """  
    id = deviceId
    policy = request.get_json()
    if request.method == 'DELETE':
        isDeletePolicy = True
    else:
        isDeletePolicy = False
    #######
    code, message, data = stub.setPolicy(id, True, policy,isDeletePolicy)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    #######
    ret = dict(Status=0, Message="success")
    return jsonify(ret)

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
                        [ { "device_id": 0, "poliyList": [ { "action": { "type": "POLICY_ACTION_TYPE_NULL" }, "condition": { "threshold": 11, "type": "POLICY_CONDITION_TYPE_LESS" }, "device_id": 0, "notifyCallBackUrl": "http://abc.test.com:6443", "type": "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS" } ] } ]
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "There is no data", "Status": 1 }
    """  
    id = groupId
    ret = []
    code, message, data = stub.getPolicy(id, False)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    retOne = {}
    retOne["group_id"] = id
    retOne["poliyList"] = data
    ret.append(retOne)
    return jsonify(ret)

def set_group_policy(groupId):
    """
    Set a policy for a group
    ---
    post:
        tags:
            - "Policy"
        description: Set a policy for a group. The isDeletePolicy must be false.
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
                        { "Message": "success", "Status": 0 }
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "Invalid Parameter: policy type is invalid.", "Status": 1 }
    delete:
        tags:
            - "Policy"
        description: Delete a policy for a group. The policy type must be set, isDeletePolicy must be true.
        parameters:
            - 
                name: policy info
                in: body
                description: policy info which will be deleted from a group. the policy type must be set, isDeletePolicy must be true.
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
                        { "Message": "success", "Status": 0 }
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "Invalid Parameter: policy type is invalid.", "Status": 1 }
    """  
    id = groupId
    policy = request.get_json()
    if request.method == 'DELETE':
        isDeletePolicy = True
    else:
        isDeletePolicy = False
    #######
    code, message, data = stub.setPolicy(id, False, policy,isDeletePolicy)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    #######
    ret = dict(Status=0, Message="success")
    return jsonify(ret)

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
                        [ { "device_id": 0, "poliyList": [ { "action": { "type": "POLICY_ACTION_TYPE_NULL" }, "condition": { "threshold": 11, "type": "POLICY_CONDITION_TYPE_LESS" }, "device_id": 0, "notifyCallBackUrl": "http://abc.test.com:6443", "type": "POLICY_TYPE_RAS_ERROR_CAT_PROGRAMMING_ERRORS" } ] } ]
            500:
                description: Error
                schema: PolicyRespSchema
                examples: 
                    application/json:
                        { "Message": "There is no data", "Status": 1 }
    """  
    code, message, data = stub.getDeviceList()
    if code != 0:
        error = dict(Status=3, Message=message)
        return jsonify(error), 500
    #####
    ret = []
    #####
    for one in data:
        id = one["device_id"]
        code, message, data = stub.getPolicy(id, True)
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
        retOne = {}
        retOne["device_id"] = id
        retOne["poliyList"] = data
        ret.append(retOne)
    #####
    return jsonify(ret)

def policyCallBackThread():
    while True:
        try:
            stub.readPolicyNotifyData()
        except:
            time.sleep(5)
            #traceback.print_exc()  
            time_stamp = datetime.datetime.now()
            time_stamp = time_stamp.strftime('[%Y-%m-%d %H:%M:%S]')
            print("{}: policyCallBackThread(): Failed to readPolicyNotifyData!".format(time_stamp))
        

def startPolicyCallBackThread():
    t = threading.Thread(target=policyCallBackThread, name='policyCallBackRun')
    t.daemon = True
    t.start()
    pass