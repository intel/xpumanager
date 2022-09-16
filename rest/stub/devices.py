#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file devices.py
#

from google.protobuf import empty_pb2
import uuid
import core_pb2
from .grpc_stub import stub, exit_on_disconnect


@exit_on_disconnect
def getDeviceList():
    resp = stub.getDeviceList(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = []
    for d in resp.info:
        device = dict()
        device['device_id'] = d.id.id
        device['device_type'] = "GPU" if d.type.value == 0 else "Unknown"
        device['uuid'] = str(uuid.UUID(d.uuid))
        device['device_name'] = d.deviceName
        device['pci_device_id'] = d.pcieDeviceId
        device['pci_bdf_address'] = d.pciBdfAddress
        device['vendor_name'] = d.vendorName
        device['drm_device'] = d.drmDevice
        device['@odata.id'] = "/rest/v1/devices/{}".format(d.id.id)
        data.append(device)
    return 0, "OK", data


def getDeviceProperties(deviceId, username="", password=""):
    resp = stub.getDeviceProperties(core_pb2.DeviceId(id=deviceId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    for prop in resp.properties:
        data[prop.name.lower()] = prop.value
    # device_id
    data["device_id"] = deviceId
    # serial number
    resp = stub.getDeviceSerialNumberAndAmcFwVersion(core_pb2.GetDeviceSerialNumberRequest(
        deviceId=deviceId, username=username, password=password))
    if resp.serialNumber:
        data["serial_number"] = resp.serialNumber
    if resp.amcFwVersion:
        data["amc_firmware_version"] = resp.amcFwVersion
    # links
    data["health"] = {
        "@odata.id": "/rest/v1/devices/{}/health".format(deviceId)}
    data["topology"] = {
        "@odata.id": "/rest/v1/devices/{}/topology".format(deviceId)}
    return 0, "OK", data


def getAMCFirmwareVersions(username="", password=""):
    resp = stub.getAMCFirmwareVersions(
        core_pb2.GetAMCFirmwareVersionsRequest(username=username, password=password))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    versions = [v for v in resp.versions]
    data = dict(amc_fw_version=versions)
    return 0,"OK",data