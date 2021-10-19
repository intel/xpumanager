from flask import request, jsonify
import stub


def run_firmware_flash(deviceId):
    firmwareType = request.form.get('type', type=int, default=0)
    filePath = request.form.get('file', type=str, default='')
    rc = stub.runFirmwareFlash(deviceId, firmwareType, filePath)

    return jsonify({'result': rc})


def get_firmware_flash_result(deviceId):
    firmwareType = request.args.get('type', type=int, default=0)

    rc = stub.getFirmwareFlashResult(deviceId, firmwareType)

    return jsonify({'result': rc})
