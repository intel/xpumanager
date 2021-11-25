from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class FirmwareFlashJobSchema(Schema):
    #type = fields.Str(metadata={"description": "The firmware type to flash, GSC"})
    file = fields.Str(metadata={"description": "The path of firmware binary file to flash"})


class FirmwareFlashStateQuerySchema(Schema):
    #type = fields.Str(metadata={"description": "The firmware type"})
    pass


class FirmwareFlashResultSchema(Schema):
    result = fields.Str(metadata={"description": "Firmware flash state, OK/FAILED/ONGOING"})


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
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
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
        return jsonify({'error': 'missing arguments'})

    filePath = req.get('file')
    if not filePath:
        return jsonify({'error': 'missing arguments'})

    rc = stub.runFirmwareFlash(deviceId, 0, filePath)
    return jsonify({'result': rc})


def get_firmware_flash_result(deviceId):
    """Get firmware flash state
    ---
    get:
        tags:
            - "Firmware Flash"
        description: Get firmware flash state
        consumes:
            - text/plain; charset=utf-8
            - application/json
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: FirmwareFlashResultSchema
            500:
                description: Error
    """
    #firmwareType = request.args.get('type', type=str, default='')

    rc = stub.getFirmwareFlashResult(deviceId, 0)

    return jsonify({'result': rc})
