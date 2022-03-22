#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file agent_settings.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields, validate, ValidationError


class AgentConfigInfoSchema(Schema):
    sample_interval = fields.Int(
        strict=True,
        required=False,
        validate=validate.OneOf([100, 200, 500, 1000]),
        metadata={
            "description": "Agent sample interval, in milliseconds, options are [100, 200, 500, 1000]"})


configNameToTypeDict = {
    "sample_interval": {
        "configType": "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL",
        "valueType": "int64"
    }
}


def findNameByConfigType(configType):
    for k in configNameToTypeDict:
        if configType == configNameToTypeDict[k]["configType"]:
            return k
    return None


def agent_setting():
    """ Agent settings
    ---
    get:
        tags:
            - "Agent Setting"
        description: Get XPUM settings
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: AgentConfigInfoSchema
            400:
                description: Error
    post:
        tags:
            - "Agent Setting"
        description: Modify XPUM settings
        consumes:
            - application/json
        parameters:
            - 
                name: modify info
                in: body
                description: 
                schema: AgentConfigInfoSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: AgentConfigInfoSchema
            400:
                description: Error
    """
    if request.method == 'GET':
        code, message, data = stub.getAllAgentConfig()
        if code != 0:
            error = dict(status=code, message=message)
            return jsonify(error), 500
        respData = dict()
        for entry in data:
            name = findNameByConfigType(entry["name"])
            value = entry["value"]
            respData[name] = value
        return jsonify(respData), 200
    elif request.method == 'POST':
        req = request.get_json()
        try:
            schema = AgentConfigInfoSchema()
            schema.load(req)
        except ValidationError as err:
            return jsonify(err.messages), 400
        params = []
        for k in req:
            d = configNameToTypeDict[k]
            param = dict(
                value=req[k], name=d["configType"], valueType=d["valueType"])
            params.append(param)
        code, message, data = stub.setAgentConfig(params)
        if code != 0:
            error = dict(status=code, message=message, error_list=data)
            return jsonify(error), 500
        config_info = dict()
        for entry in data:
            name = findNameByConfigType(entry["name"])
            value = entry["value"]
            config_info[name] = value
        return jsonify(config_info), 200
