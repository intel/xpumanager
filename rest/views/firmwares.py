from flask import request, jsonify
import stub


def run_firmware_flash(deviceId):
    req = request.get_json();
    firmwareType = req.get('type');
    if not firmwareType:
        firmwareType = 0

    filePath = req.get('file');
    if not filePath:
        return jsonify({'error': 'missing arguments'})
    
    rc = stub.runFirmwareFlash(deviceId, firmwareType, filePath)
    return jsonify({'result': rc})


def get_firmware_flash_result(deviceId):
    firmwareType = request.args.get('type', type=int, default=0)

    rc = stub.getFirmwareFlashResult(deviceId, firmwareType)

    return jsonify({'result': rc})
