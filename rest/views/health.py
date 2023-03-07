#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file health.py
#

import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields


class ThresholdSchema(Schema):
    custom_threshold = fields.Int(
        metadata={"description": "The custom threshold for coreTemperature in celsius degree, memoryTemperature in celsius degree or power in watts"})


class HealthComponentSchema(Schema):
    description = fields.Str(
        metadata={"description": "The description for health"})
    status = fields.Int(
        metadata={"description": "The status for health"})


class HealthConfigurableComponentSchemaBase(Schema):
    description = fields.Str(
        metadata={"description": "The description for health"})
    status = fields.Int(
        metadata={"description": "The status for health"})
    throttle_threshold = fields.Int(
        metadata={"description": "The throttle threshold in watts for health"})
    custom_threshold = fields.Int(
        metadata={"description": "The custom threshold in watts for health"})


class HealthConfigurableComponentSchemaExt(Schema):
    description = fields.Str(
        metadata={"description": "The description for health"})
    status = fields.Int(
        metadata={"description": "The status for health"})
    throttle_threshold = fields.Int(
        metadata={"description": "The throttle threshold in celsius degree for health"})
    shutdown_threshold = fields.Int(
        metadata={"description": "The shutdown threshold in celsius degree for health"})
    custom_threshold = fields.Int(
        metadata={"description": "The custom threshold in celsius degree for health"})


class HealthSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    core_temperature = fields.Nested(HealthConfigurableComponentSchemaExt)
    memory_temperature = fields.Nested(HealthConfigurableComponentSchemaExt)
    power = fields.Nested(HealthConfigurableComponentSchemaBase)
    memory = fields.Nested(HealthComponentSchema)
    xe_link_port = fields.Nested(HealthComponentSchema)
    frequency = fields.Nested(HealthComponentSchema)


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
            404:
                description: Device not found
            500:
                description: Error
    """
    code, message, data = stub.getHealth(deviceId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "device not found":
            return jsonify(error), 404
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
            404:
                description: Group not found
            500:
                description: Error
    """
    code, message, data = stub.getHealthByGroup(groupId, "All")
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "group not found":
            return jsonify(error), 404
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
                description: Health type, coreTemperature, memoryTemperature, power, memory, xeLinkPort or frequency
                type: str
        responses:
            200:
                description: OK
                schema: HealthSchema
            404:
                description: Device not found or health type not supported
            500:
                description: Error
    """
    if healthType not in ["coreTemperature", "memoryTemperature", "power", "memory", "xeLinkPort", "frequency"]:
        error = dict(Status=1, Message="health type not supported")
        return jsonify(error), 404

    code, message, data = stub.getHealth(deviceId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "device not found":
            return jsonify(error), 404
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
                description: Health type, coreTemperature, memoryTemperature, power, memory, xeLinkPort or frequency
                type: str
        responses:
            200:
                description: OK
                schema: HealthGroupSchema
            404:
                description: Group not found or health type not supported
            500:
                description: Error
    """
    if healthType not in ["coreTemperature", "memoryTemperature", "power", "memory", "xeLinkPort", "frequency"]:
        error = dict(Status=1, Message="health type not supported")
        return jsonify(error), 404

    code, message, data = stub.getHealthByGroup(groupId, healthType)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "group not found":
            return jsonify(error), 404
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
                name: custom_threshold
                in: body
                description: The custom threshold for coreTemperature, memoryTemperature or power
                schema: ThresholdSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: healthType
                in: path
                description: Health type, only coreTemperature, memoryTemperature or power
                type: str
        responses:
            200:
                description: OK
            400:
                description: Bad Request, for example invalid threshold
            404:
                description: Device not found or health type not supported
            500:
                description: Error
    """
    if healthType not in ["coreTemperature", "memoryTemperature", "power"]:
        error = dict(Status=1, Message="health type not supported")
        return jsonify(error), 404

    req = request.get_json()
    threshold = req["custom_threshold"]
    code, message, data = stub.setHealthConfig(deviceId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "invalid threshold":
            return jsonify(error), 400
        elif message == "device not found":
            return jsonify(error), 404
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
                name: custom_threshold
                in: body
                description: The custom threshold for coreTemperature, memoryTemperature or power
                schema: ThresholdSchema
            -
                name: groupId
                in: path
                description: Group id
                type: integer
            -
                name: healthType
                in: path
                description: health type, only coreTemperature, memoryTemperature or power
                type: str
        responses:
            200:
                description: OK
            400:
                description: Bad Request, for example invalid threshold
            404:
                description: Group not found or health type not supported
            500:
                description: Error
    """
    if healthType not in ["coreTemperature", "memoryTemperature", "power"]:
        error = dict(Status=1, Message="health type not supported")
        return jsonify(error), 404

    req = request.get_json()
    threshold = req["custom_threshold"]
    code, message, data = stub.setHealthConfigByGroup(
        groupId, healthType, threshold)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "invalid threshold":
            return jsonify(error), 400
        elif message == "group not found":
            return jsonify(error), 404
        return jsonify(error), 500
    return jsonify(data)
