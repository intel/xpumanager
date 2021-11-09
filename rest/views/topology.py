from flask import request, jsonify
import stub
from marshmallow import Schema, fields

class TopologyInfoSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})

def get_topology(deviceId):
    """
    Get device topology.
    ---
    get:
        tags:
            - "Topology"
        description: Get device topology
        parameters: 
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
                    items: TopologyInfoSchema
            500:
                description: Error
    """
    code, message, data = stub.getTopology(deviceId)
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400
    
