import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields

class ThresholdSchema(Schema):
    threshold = fields.Int(
        metadata={"description": "The threshold for power or temperature"})

class HealthComponentSchema(Schema):
    description = fields.Str(
        metadata={"description": "The description for health"})
    status = fields.Int(
        metadata={"description": "The status for health"})

class HealthConfigurableComponentSchema(Schema):
    description = fields.Str(
        metadata={"description": "The description for health"})
    status = fields.Int(
        metadata={"description": "The status for health"})
    threshold = fields.Int(
        metadata={"description": "The threshold for health, only for power and temperature"})

class HealthSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    power = fields.Nested(HealthConfigurableComponentSchema)
    temperature = fields.Nested(HealthConfigurableComponentSchema)
    memory = fields.Nested(HealthComponentSchema)
    fabricPort = fields.Nested(HealthComponentSchema)


def get_health_all(deviceId):
    """
    Get health for device
    ---
    get:
        tags:
            - "Health"
        description: Get health for device
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
                schema: HealthSchema
            500:
                description: Error
    """
    code, message, data = stub.getHealth(deviceId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


class HealthGroupSchema(Schema):
    group_id = fields.Int(metadata={"description": "Group id"})
    device_count = fields.Int(metadata={"description": "Device count"})
    device_list = fields.List(fields.Nested(HealthSchema))


def get_group_health_all(groupId):
    """
    Get health for group
    ---
    get:
        tags:
            - "Health"
        description: Get health for group
        parameters:
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        responses:
            200:
                description: OK
                schema: HealthGroupSchema
            500:
                description: Error
    """
    code, message, data = stub.getHealthByGroup(groupId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_health(deviceId, healthType):
    """
    Get specific health for device
    ---
    get:
        tags:
            - "Health"
        description: Get specific health for device and response JSON object only contains targeted-type health
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: healthType
                in: path
                description: Health type, temperature, power, memory or fabricPort
                type: str
        responses:
            200:
                description: OK
                schema: HealthSchema
            500:
                description: Error
    """
    if healthType not in ["temperature", "power", "memory", "fabricPort"]:
        return

    code, message, data = stub.getHealth(deviceId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_group_health(groupId, healthType):
    """
    Get specific health for group
    ---
    get:
        tags:
            - "Health"
        description: Get health for group and response JSON object only contains targeted-type health
        parameters:
            -
                name: groupId
                in: path
                description: Group id
                type: integer
            -
                name: healthType
                in: path
                description: Health type, temperature, power, memory or fabricPort
                type: str
        responses:
            200:
                description: OK
                schema: HealthGroupSchema
            500:
                description: Error
    """
    if healthType not in ["temperature", "power", "memory", "fabricPort"]:
        return

    code, message, data = stub.getHealthByGroup(groupId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_health_config(deviceId, healthType):
    """
    Set health config for device
    ---
    put:
        tags:
            - "Health"
        description: Set health config for device
        parameters:
            - 
                name: threshold
                in: body
                description: The threshold for power or temperature
                schema: ThresholdSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: healthType
                in: path
                description: Health type, only power or temperature
                type: str
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if healthType not in ["temperature", "power"]:
        return

    req = request.get_json()
    threshold = req["threshold"]
    code, message, data = stub.setHealthConfig(deviceId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_group_health_config(groupId, healthType):
    """
    Set health config for group
    ---
    put:
        tags:
            - "Health"
        description: Set health config for group
        parameters:
            - 
                name: threshold
                in: body
                description: The threshold for power or temperature
                schema: ThresholdSchema
            -
                name: groupId
                in: path
                description: Group id
                type: integer
            -
                name: healthType
                in: path
                description: health type, only power or temperature
                type: str
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if healthType not in ["temperature", "power"]:
        return

    req = request.get_json()
    threshold = req["threshold"]
    code, message, data = stub.setHealthConfigByGroup(
        groupId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
