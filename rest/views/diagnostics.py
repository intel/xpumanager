#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file diagnostics.py
#

import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields


class DiagnosticsLevelSchema(Schema):
    level = fields.Int(
        metadata={"description": "The level for diagnostics to run"})


def run_diagnostics(deviceId):
    """
    Run diagnostics for device
    ---
    post:
        tags:
            - "Diagnostics"
        description: Run diagnostics for device
        parameters:
            - 
                name: diagnostics level
                in: body
                description: The level for diagnostics to run
                schema: DiagnosticsLevelSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            201:
                description: OK
            400:
                description: Bad Request, for example invalid level
            404:
                description: Device not found
            500:
                description: Error
    """
    req = request.get_json()
    level = req["level"]
    code, message, data = stub.runDiagnostics(deviceId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "device not found":
            return jsonify(error), 404
        elif message == "invalid level":
            return jsonify(error), 400
        return jsonify(error), 500
    return jsonify(data), 201


def run_group_diagnostics(groupId):
    """
    Run diagnostics for group
    ---
    post:
        tags:
            - "Diagnostics"
        description: Run diagnostics for group
        parameters:
            - 
                name: diagnostics level
                in: body
                description: The level for diagnostics to run
                schema: DiagnosticsLevelSchema
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        responses:
            201:
                description: OK
            400:
                description: Bad Request, for example invalid level
            404:
                description: Group not found
            500:
                description: Error
    """
    req = request.get_json()
    level = req["level"]
    code, message, data = stub.runDiagnosticsByGroup(groupId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "group not found":
            return jsonify(error), 404
        elif message == "invalid level":
            return jsonify(error), 400
        return jsonify(error), 500
    return jsonify(data), 201


class DiagnosticsComponentSchema(Schema):
    component_type = fields.Str(metadata={"description": "Component type"})
    finished = fields.Boolean(
        metadata={"description": "Finished or not"})
    result = fields.Str(
        metadata={"description": "Result status"})
    message = fields.Str(
        metadata={"description": "Result message"})


class DiagnosticsInfoSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    level = fields.Int(
        metadata={"description": "The level for diagnostics to run"})
    finished = fields.Boolean(
        metadata={"description": "Finished or not"})
    message = fields.Str(
        metadata={"description": "Result message"})
    result = fields.Str(
        metadata={"description": "Result status"})
    component_count = fields.Int(metadata={"description": "Component count"})
    start_time = fields.Str(metadata={"description": "Start time"})
    end_time = fields.Str(metadata={"description": "End time"})
    component_list = fields.List(fields.Nested(DiagnosticsComponentSchema))


def get_diagnostics_result(deviceId):
    """
    Get diagnostics result for device
    ---
    get:
        tags:
            - "Diagnostics"
        description: Get diagnostics result for device
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
                schema: DiagnosticsInfoSchema
            404:
                description: Device not found
            500:
                description: Error
    """
    code, message, data = stub.getDiagnosticsResult(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "device not found":
            return jsonify(error), 404
        return jsonify(error), 500
    return jsonify(data)


class DiagnosticsGroupInfoSchema(Schema):
    group_id = fields.Int(metadata={"description": "Group id"})
    device_count = fields.Int(metadata={"description": "Device count"})
    finished = fields.Boolean(metadata={"description": "Finished or not"})
    device_list = fields.List(fields.Nested(DiagnosticsInfoSchema))


def get_group_diagnostics_result(groupId):
    """
    Get diagnostics result for group
    ---
    get:
        tags:
            - "Diagnostics"
        description: Get diagnostics result for group
        parameters:
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        responses:
            200:
                description: OK
                schema: DiagnosticsGroupInfoSchema
            404:
                description: Group not found
            500:
                description: Error
    """
    code, message, data = stub.getDiagnosticsResultByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        if message == "group not found":
            return jsonify(error), 404
        return jsonify(error), 500
    return jsonify(data)
