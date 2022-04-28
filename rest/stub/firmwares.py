#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file firmwares.py
#

from .grpc_stub import stub
import core_pb2
from .xpum_enums import XpumResult


def runFirmwareFlash(deviceId, firmwareType, filePath):
    job = core_pb2.XpumFirmwareFlashJob()
    job.id.id = deviceId
    if firmwareType == 'GSC':
        job.type.value = 0
    elif firmwareType == 'AMC':
        job.type.value = 1
    else:
        job.type.value = 2
    job.path = filePath
    resp = stub.runFirmwareFlash(job).type
    if resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC'].value:
        return 1, "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.", None
    elif resp.value == XpumResult['XPUM_RESULT_DEVICE_NOT_FOUND'].value:
        return resp.value, "Device not found.", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND'].value:
        return resp.value, "Firmware image not found.", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_ILLEGAL_FILENAME'].value:
        return resp.value, "Illegal firmware image filename. Image filename should not contain following characters: {}()><&*'|=?;[]$-#~!\"%:+,`", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE'].value:
        return resp.value, "Updating AMC firmware on single device is not supported", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GSC_ALL'].value:
        return resp.value, "Updating GSC firmware on all devices is not supported", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE'].value:
        return resp.value, "Device models are inconsistent, failed to upgrade all.", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND'].value:
        return resp.value, "Igsc tool doesn't exit.", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_TASK_RUNNING'].value:
        return resp.value, "Firmware update task already running.", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE'].value:
        return resp.value, "The image file is not a right SOC FW image file.", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE'].value:
        return resp.value, "The image file is a right SOC FW image file, but not proper for the target GPU.", None
    elif resp.value != XpumResult['XPUM_OK'].value:
        return 1, "Fail to run firmware flash", None
    else:
        data = dict()
        data['result'] = 'OK'
        return 0, "OK", data


def getFirmwareFlashResult(deviceId, firmwareType):
    request = core_pb2.XpumFirmwareFlashTaskRequest()
    request.id.id = deviceId
    if firmwareType == 'GSC':
        request.type.value = 0
    elif firmwareType == 'AMC':
        request.type.value = 1
    else:
        request.type.value = 2
    resp = stub.getFirmwareFlashResult(request)

    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    else:
        data = dict()
        if resp.result.value == 0:
            data['result'] = 'OK'
        elif resp.result.value == 1:
            data['result'] = 'FAILED'
        else:
            data['result'] = 'ONGOING'

        return 0, "OK", data
