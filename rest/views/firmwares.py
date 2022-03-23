#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file firmwares.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class FirmwareFlashJobSchema(Schema):
    file = fields.Str(metadata={"description": "The path of firmware binary file to flash"})
    firmware_name = fields.Str(metadata={"description": "Firmware name, options are: GSC, AMC"})

class FirmwareFlashResultSchema(Schema):
    result = fields.Str(metadata={"description": "Firmware flash state, OK/FAILED/ONGOING"})

def run_firmware_flash_all():
    return run_firmware_flash(1024)

def run_firmware_flash(deviceId):
    """Run firmware flash
    ---
    post:
        tags:
            - "Firmware Flash"
        description: Run firmware flash
        consumes:
            - application/json
        parameters:
            - 
                name: Firmware flash job
                in: body
                description: Information needed to flash firmware
                schema: FirmwareFlashJobSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: FirmwareFlashResultSchema
            500:
                description: Error
    """
    req = request.get_json()
    if not req:
        return jsonify({'error': 'missing arguments'}), 400

    filePath = req.get('file')
    if not filePath:
        return jsonify({'error': 'missing arguments'}), 400
    pos = filePath.find("/")
    if pos == -1:
        return jsonify({'error': 'invalid file path, only full path supported'}), 400
    else:
       filePath = filePath[pos:]    # trunc the file path
    
    fwType = req.get('firmware_name')
    if not fwType:
        return jsonify({'error': 'missing arguments'}), 400
    if not fwType == 'GSC' and not fwType == 'AMC':
        return jsonify({'error': 'invalid firmware name'}), 400
    if fwType == 'GSC' and deviceId == 1024:
        return jsonify({'error': 'upgrading GSC firmware on all devices is not supported'}), 400
    if fwType == 'AMC' and not deviceId == 1024:
        return jsonify({'error': 'upgrading AMC firmware only supports on single device'}), 400

    firmwareType = 0 if fwType == 'GSC' else 1

    code, msg, data = stub.runFirmwareFlash(deviceId, firmwareType, filePath)
    if code == stub.XpumResult['XPUM_UPDATE_FIRMWARE_GFXFWFPT_NOT_FOUND'].value:
        return jsonify({'error': msg}), 500
    elif code != 0:
        return jsonify({'error': msg}), 400
    return jsonify({'result': msg})

def get_firmware_flash_result_all():
    return get_firmware_flash_result(1024)

def get_firmware_flash_result(deviceId):
    """Get firmware flash state
    ---
    get:
        tags:
            - "Firmware Flash"
        description: Get firmware flash state
        consumes:
            - text/plain; charset=utf-8
        parameters:
            -
                name: firmware_name
                in: query
                description: "Firmware name, options are: GSC, AMC"
                type: string
                enum: [GSC,AMC]
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: FirmwareFlashResultSchema
            500:
                description: Error
    """
    fwType = request.args.get('firmware_name', type=str, default='')
    if fwType == '':
        return jsonify({'error': 'missing arguments'})
    if not fwType == 'GSC' and not fwType == 'AMC':
        return jsonify({'error': 'invalid firmware name'})

    if fwType == "GSC" and deviceId == 1024:
        return jsonify({'error': 'upgrading GSC firmware on all devices is not supported'})
    if fwType == "AMC" and not deviceId == 1024:
        return jsonify({'error': 'upgrading AMC firmware only supports on single device'})

    firmwareType = 0 if fwType == 'GSC' else 1

    rc = stub.getFirmwareFlashResult(deviceId, firmwareType)

    return jsonify({'result': rc})
