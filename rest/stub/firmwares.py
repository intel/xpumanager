#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file firmwares.py
#

from .grpc_stub import stub
import core_pb2
from .xpum_enums import XpumResult, XpumFirmwareType, XpumFirmwareFlashResultType


def runFirmwareFlash(deviceId, firmwareType, filePath, username="", password=""):
    job = core_pb2.XpumFirmwareFlashJob()
    job.id.id = deviceId
    if firmwareType == 'GFX':
        job.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_GFX"].value
    elif firmwareType == 'AMC':
        job.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_AMC"].value
        job.username = username
        job.password = password
    elif firmwareType == 'GFX_DATA':
        job.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_GFX_DATA"].value
    elif firmwareType == 'GFX_PSCBIN':
        job.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_GFX_PSCBIN"].value
    else:
        return 1, "Unknown firmware type", None
    job.path = filePath
    resp = stub.runFirmwareFlash(job)
    err_code = resp.errorNo
    err_msg = resp.errorMsg
    if err_code == XpumResult['XPUM_OK'].value:
        data = dict()
        data['result'] = 'OK'
        return 0, "OK", data
    if err_code == XpumResult['XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE'].value:
        return err_code, "Device models are inconsistent, failed to upgrade all.", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND'].value:
        return err_code, "Firmware image not found.", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND'].value:
        return err_code, "Igsc tool doesn't exit.", None
    elif err_code == XpumResult['XPUM_RESULT_DEVICE_NOT_FOUND'].value:
        return err_code, "Device not found.", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL'].value:
        if firmwareType == 'GFX':
            return err_code, "Updating GFX firmware on all devices is not supported", None
        elif firmwareType == 'GFX_DATA':
            return err_code, "Updating GFX_DATA firmware on all devices is not supported", None
        else:
            return err_code, "Updating GFX_PSCBIN firmware on all devices is not supported", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE'].value:
        return err_code, "Updating AMC firmware on single device is not supported", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_TASK_RUNNING'].value:
        return err_code, "Firmware update task already running.", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE'].value:
        return err_code, "The image file is not a right FW image file.", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE'].value:
        return err_code, "The image file is a right FW image file, but not proper for the target GPU.", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_DATA'].value:
        return err_code, "The device doesn't support GFX_DATA firmware update", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC'].value:
        return err_code, "The device doesn't support PSCBIN firmware update", None
    elif err_code == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC'].value:
        return err_code, "Installed igsc doesn't support PSCBIN firmware update", None
    else:
        if len(err_msg):
            return 1, err_msg, None
        else:
            return 1, "Fail to run firmware flash", None


def getFirmwareFlashResult(deviceId, firmwareType, username="", password=""):
    request = core_pb2.XpumFirmwareFlashTaskRequest()
    request.id.id = deviceId
    if firmwareType == 'GFX':
        request.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_GFX"].value
    elif firmwareType == 'AMC':
        request.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_AMC"].value
        request.username = username
        request.password = password
    elif firmwareType == 'GFX_DATA':
        request.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_GFX_DATA"].value
    elif firmwareType == 'GFX_PSCBIN':
        request.type.value = XpumFirmwareType["XPUM_DEVICE_FIRMWARE_GFX_PSCBIN"].value
    else:
        return 1, "Unknown firmware type", None

    resp = stub.getFirmwareFlashResult(request)

    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    else:
        data = dict()
        if resp.result.value == XpumFirmwareFlashResultType["XPUM_DEVICE_FIRMWARE_FLASH_OK"].value:
            data['result'] = 'OK'
        elif resp.result.value == XpumFirmwareFlashResultType["XPUM_DEVICE_FIRMWARE_FLASH_ERROR"].value:
            data['result'] = 'FAILED'
        elif resp.result.value == XpumFirmwareFlashResultType["XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED"].value:
            data['result'] = 'UNSUPPORTED'
        else:
            data['result'] = 'ONGOING'

        return 0, "OK", data
