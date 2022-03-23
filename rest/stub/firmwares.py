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
    job.type.value = firmwareType
    job.path = filePath
    resp = stub.runFirmwareFlash(job)
    if resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC'].value:
        return 1, "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.", None
    elif resp.value == XpumResult['XPUM_RESULT_DEVICE_NOT_FOUND'].value:
        return resp.value, "Device not found", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND'].value:
        return resp.value, "Image file not found", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_ILLEGAL_FILENAME'].value:
        return resp.value, "Illegal image filename", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE'].value:
        return resp.value, "upgrading AMC firmware only supports on single device", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GSC_ALL'].value:
        return resp.value, "upgrading GSC firmware on all devices is not supported", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE'].value:
        return resp.value, "The GPU models are inconsistence", None
    elif resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_GFXFWFPT_NOT_FOUND'].value:
        return resp.value, "GfxFwFPT tool not found", None
    elif resp.value != XpumResult['XPUM_OK'].value:
        return 1, "Fail to run firmware flash", None
    else:
        data = dict()
        data['run_firmware_flash_result'] = 'OK'
        return 0, "OK", data


def getFirmwareFlashResult(deviceId, firmwareType):
    request = core_pb2.XpumFirmwareFlashTaskRequest()
    request.id.id = deviceId
    request.type.value = firmwareType
    resp = stub.getFirmwareFlashResult(request)

    if len(resp.errorMsg) != 0:
        return 1, "Fail to get firmware flash result", None
    else:
        data = dict()
        data['device_id'] = resp.id.id
        if resp.type.value == 0:
            data['firmware_name'] = 'GSC'
        else:
            data['firmware_name'] = 'AMC'

        if resp.result.value == 0:
            data['firmware_flash_result'] = 'OK'
        elif resp.result.value == 1:
            data['firmware_flash_result'] = 'FAILED'
        else:
            data['firmware_flash_result'] = 'ONGOING'

        return 0, "OK", data
