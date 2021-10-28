import stub
from flask import request
from flask import jsonify
from marshmallow import Schema, fields

class DiagnosticsLevelSchema(Schema):
    diagnostics_level = fields.Int(
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
            200:
                description: OK
            500:
                description: Error
    """
    req = request.get_json()
    level = req["diagnostics_level"]
    code, message, data = stub.runDiagnostics(deviceId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


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
            200:
                description: OK
            500:
                description: Error
    """
    req = request.get_json()
    level = req["diagnostics_level"]
    code, message, data = stub.runDiagnosticsByGroup(groupId, level)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


class DiagnosticsComponentSchema(Schema):
    component_type = fields.Int(metadata={"description": "Component type"})
    finished = fields.Boolean(
        metadata={"description": "Finished or not"})
    result = fields.Str(
        metadata={"description": "Result status"})
    message = fields.Str(
        metadata={"description": "Result message"})


class DiagnosticsInfoSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    diagnostics_level = fields.Int(
        metadata={"description": "The level for diagnostics to run"})
    finished = fields.Boolean(
        metadata={"description": "Finished or not"})
    message = fields.Str(
        metadata={"description": "Result message"})
    component_count = fields.Int(metadata={"description": "Component count"})
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
            500:
                description: Error
    """
    code, message, data = stub.getDiagnosticsResult(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


class DiagnosticsGroupInfoSchema(Schema):
    group_id = fields.Int(metadata={"description": "Group id"})
    device_count = fields.Int(metadata={"description": "Device count"})
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
            500:
                description: Error
    """
    code, message, data = stub.getDiagnosticsResultByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
