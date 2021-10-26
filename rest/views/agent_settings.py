from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class AgentSettingInfoSchema(Schema):
    sample_interval = fields.Int(metadata={
                                 "description": "Agent sample interval, in milliseconds, options are [100, 200, 500, 1000]"})



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
                schema: AgentSettingInfoSchema
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
                schema: AgentSettingInfoSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: AgentSettingInfoSchema
            400:
                description: Error
    """
    if request.method == 'GET':
        code, message, data = stub.getAllAgentConfig()
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        return jsonify(data), 200
    elif request.method == 'POST':
        req = request.get_json()
        if "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL" in req:
            value = req["XPUM_AGENT_CONFIG_SAMPLE_INTERVAL"]
            code, message, data = stub.setAgentConfig(
                "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL", value)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
            return "", 200
        else:
            return dict(Status=1, Message="Illegal key"), 400
