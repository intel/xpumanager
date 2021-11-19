import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields
import time, threading
import traceback,datetime

class PolicyComponentSchema(Schema):
    description = fields.Str(
        metadata={"description": "The description for policy"})
    status = fields.Int(
        metadata={"description": "The status for policy"})

class PolicyConfigurableComponentSchema(Schema):
    description = fields.Str(
        metadata={"description": "The description for health"})
    status = fields.Int(
        metadata={"description": "The status for health"})
    threshold = fields.Int(
        metadata={"description": "The threshold for health, only for power and temperature"})

class PolicySchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    power = fields.Nested(PolicyConfigurableComponentSchema)
    temperature = fields.Nested(PolicyConfigurableComponentSchema)
    memory = fields.Nested(PolicyComponentSchema)
    fabricPort = fields.Nested(PolicyComponentSchema)


def get_device_policy(deviceId):
    """
    Get policy for device
    ---
    get:
        tags:
            - "Policy"
        description: Get policy for device
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
                schema: PolicySchema
            500:
                description: Error
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
    Get policy for device
    ---
    get:
        tags:
            - "Policy"
        description: Get policy for device
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
                schema: PolicySchema
            500:
                description: Error
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
    return jsonify(data)

class PolicyGroupSchema(Schema):
    group_id = fields.Int(metadata={"description": "Group id"})
    device_count = fields.Int(metadata={"description": "Device count"})
    device_list = fields.List(fields.Nested(PolicySchema))

def get_group_policy(groupId):
    """
    Get policy for device
    ---
    get:
        tags:
            - "Policy"
        description: Get policy for device
        parameters:
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        responses:
            200:
                description: OK
                schema: PolicyGroupSchema
            500:
                description: Error
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
    Get policy for device
    ---
    get:
        tags:
            - "Policy"
        description: Get policy for device
        parameters:
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        responses:
            200:
                description: OK
                schema: PolicyGroupSchema
            500:
                description: Error
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
    return jsonify(data)

def get_all_policy():
    """
    Get policy for all device
    ---
    get:
        tags:
            - "Policy"
        description: Get policy for device
        responses:
            200:
                description: OK
                schema: PolicyGroupSchema
            500:
                description: Error
    """
    code, message, data = stub.getDeviceList()
    if code == 0:
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
    else:
        return message, 500

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