#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file topology.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class TopologyInfoSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    affinity_localcpulist = fields.String(
        metadata={"description": "local cpu list"})
    affinity_localcpus = fields.String(metadata={"description": "local cpus"})
    switch_count = fields.Int(
        metadata={"description": "Device parent switch count"})
    switch_list = fields.List(fields.String(
        metadata={"description": "list of switch device path"}))


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


class TopologyXelinkSchema(Schema):
    local_device_id = fields.Int(metadata={"description": "Device id"})
    local_on_subdevice = fields.Boolean(
        metadata={"description": "if xelink port is located on a sub-device"})
    local_subdevice_id = fields.Int(metadata={"description": "sub-device id"})
    local_numa_index = fields.Int(metadata={"description": "NUMA node index"})
    local_cpu_affinity = fields.String(
        metadata={"description": "cpu affinity"})
    remote_device_id = fields.Int(metadata={"description": "remote Device id"})
    remote_subdevice_id = fields.Int(
        metadata={"description": "remote sub-device id"})
    link_type = fields.String(metadata={"description": "link type"})
    port_list = fields.List(fields.Int(metadata={
        "description": "port list link to remote device"}))


def get_topo_xelink():
    """
    Get xelink topology.
    ---
    get:
        tags:
            - "Topology"
        description: Get xelink topology
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: TopologyXelinkSchema
            500:
                description: Error
    """
    code, message, data = stub.getTopoXelink()
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400


class TopologyXMLSchema(Schema):
    length = fields.Int(metadata={"description": "XML buffer length"})
    xmlstring = fields.String(
        metadata={"description": "XML sting of node topology"})


def export_topology():
    """
    Export node topology xml string.
    ---
    get:
        tags:
            - "Topology"
        description: Export node topology
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: TopologyXMLSchema
            500:
                description: Error
    """
    code, message, data = stub.exportTopology()
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400
