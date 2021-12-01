from flask import request, jsonify
import stub
from marshmallow import Schema, fields

class TopologyInfoSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    affinity_localcpulist = fields.String(metadata={"description": "local cpu list"})
    affinity_localcpus = fields.String(metadata={"description": "local cpus"})
    switch_count = fields.Int(metadata={"description": "Device parent switch count"})
    switch_list = fields.String(metadata={"description": "list of switch device path"})

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
                schema: TopologyInfoSchema
            500:
                description: Error
    """
    code, message, data = stub.getTopology(deviceId)
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400
    
