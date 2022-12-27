#
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file ps.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class DeviceUtilByProcSchema(Schema):
    process_id = fields.Int(metadata={"description": "Process ID"})
    process_name = fields.String(metadata={"description": "Process Name"})
    device_id = fields.Int(metadata={"description": "Device ID"})
    mem_size = fields.Int(metadata={"description": "Memory size"})
    shared_mem_size = fields.Int(metadata={"description": "Shared memory size"})
   
class DeviceUtilByProcListSchema(Schema):
    utils = fields.Nested(DeviceUtilByProcSchema, many=True)

def get_device_util_by_proc(deviceId):
    """
    Get per process device utilization of a device.
    ---
    get:
        tags:
            - "ps"
        description: Get per process device utilization.
        parameters: 
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
                schema: DeviceUtilByProcListSchema
            500:
                description: Error
    """
    code, message, data = stub.getDeviceUtilByProc(deviceId)
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400

def get_all_device_util_by_proc():
    """
    Get per process device utilization of all devices.
    ---
    get:
        tags:
            - "ps"
        description: Get per process device utilization.
        responses:
            200:
                description: OK
                schema: DeviceUtilByProcListSchema
            500:
                description: Error
    """
    code, message, data = stub.getAllDeviceUtilByProc()
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400
