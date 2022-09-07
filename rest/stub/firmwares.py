#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file firmwares.py
#

from .grpc_stub import stub
import core_pb2
from .xpum_enums import XpumResult


def runFirmwareFlash(deviceId, firmwareType, filePath, username="", password=""):
    job = core_pb2.XpumFirmwareFlashJob()
    job.id.id = deviceId
    if firmwareType == 'GFX':
        job.type.value = 0
    elif firmwareType == 'AMC':
        job.type.value = 1
        job.username = username
        job.password = password
    else:
        job.type.value = 2
    job.path = filePath
    resp = stub.runFirmwareFlash(job)
    err_code = resp.type
    err_msg = resp.errorMsg
    if err_code.value == XpumResult['XPUM_RESULT_DEVICE_NOT_FOUND'].value:
        return err_code.value, "Device not found.", None
    # elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC'].value:
    #     return 1, "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND'].value:
        return err_code.value, "Firmware image not found.", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE'].value:
        return err_code.value, "Updating AMC firmware on single device is not supported", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL'].value:
        if firmwareType == 'GFX':
            return err_code.value, "Updating GFX firmware on all devices is not supported", None
        else:
            return err_code.value, "Updating GFX_DATA firmware on all devices is not supported", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE'].value:
        return err_code.value, "Device models are inconsistent, failed to upgrade all.", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND'].value:
        return err_code.value, "Igsc tool doesn't exit.", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_TASK_RUNNING'].value:
        return err_code.value, "Firmware update task already running.", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE'].value:
        return err_code.value, "The image file is not a right FW image file.", None
    elif err_code.value == XpumResult['XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE'].value:
        return err_code.value, "The image file is a right FW image file, but not proper for the target GPU.", None
    elif err_code.value != XpumResult['XPUM_OK'].value:
        if len(err_msg):
            return 1, err_msg, None
        else:
            return 1, "Fail to run firmware flash", None
    else:
        data = dict()
        data['result'] = 'OK'
        return 0, "OK", data


def getFirmwareFlashResult(deviceId, firmwareType, username="", password=""):
    request = core_pb2.XpumFirmwareFlashTaskRequest()
    request.id.id = deviceId
    if firmwareType == 'GFX':
        request.type.value = 0
    elif firmwareType == 'AMC':
        request.type.value = 1
        request.username = username
        request.password = password
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
