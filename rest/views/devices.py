#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file devices.py
#

import stub
from flask import jsonify, request
from marshmallow import Schema, fields, ValidationError


class DeviceBasicInfoSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    device_type = fields.Str(
        metadata={"description": "Device type, now is only GPU"})
    uuid = fields.Str(metadata={"description": "Device uuid"})
    device_name = fields.Str(metadata={"description": "Device name"})
    pci_device_id = fields.Str(
        metadata={"description": "The PCI device id of device"})
    pci_bdf_address = fields.Str(
        metadata={"description": "The PCI bdf address of device"})
    vendor_name = fields.Str(metadata={"description": "Vendor name"})
    odataid = fields.URL(
        data_key="@odata.id", metadata={"description": "Link to device detail properties"})


class DeviceListSchema(Schema):
    devices = fields.Nested(DeviceBasicInfoSchema, many=True)


def get_devices():
    """
    Get device list.
    ---
    get:
        tags:
            - "Devices"
        description: Get device list
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: DeviceListSchema
            500:
                description: Error
    """
    code, message, data = stub.getDeviceList()
    if code == 0:
        return jsonify(dict(devices=data))
    else:
        return message, 500


class DevicePropertiesLinkSchema(Schema):
    odataid = fields.URL(data_key="@odata.id",
                         metadata={"description": "Link to detail info"})


class DevicePropertiesSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    device_type = fields.Str(metadata={"description": "Device type"})
    device_name = fields.Str(metadata={"description": "Device name"})
    vendor_name = fields.Str(metadata={"description": "Vendor name"})
    uuid = fields.Str(metadata={"description": "Device uuid"})
    pci_device_id = fields.Str(
        metadata={"description": "The PCI device id of device"})
    pci_sub_device_id = fields.Str(
        metadata={"description": "The PCI sub device id of device"})
    pci_vendor_id = fields.Str(
        metadata={"description": "The PCI vendor id of device"})
    pci_bdf_address = fields.Str(
        metadata={"description": "The PCI bdf address of device"})
    pci_slot = fields.Str(metadata={"description": "PCI slot of device"})
    pcie_generation = fields.Str(metadata={"description": "PCIe generation"})
    pcie_max_link_width = fields.Str(
        metadata={"description": "PCIe max link width"})
    socket_id = fields.Str(metadata={"description": "socket id of OAM GPU"})
    device_stepping = fields.Str(
        metadata={"description": "The stepping of device"})
    driver_version = fields.Str(metadata={"description": "The driver version"})
    kernel_version = fields.Str(metadata={"description": "Linux kernel version"})
    firmware_name = fields.Str(
        metadata={"description": "The GFX firmware name of device"})
    firmware_version = fields.Str(
        metadata={"description": "The GFX firmware version of device"})
    serial_number = fields.Str(metadata={"description": "Serial number"})
    core_clock_rate_mhz = fields.Str(
        metadata={"description": "Clock rate for device core, in MHz"})
    memory_physical_size_byte = fields.Str(
        metadata={"description": "Device physical memory size, in bytes"})
    memory_free_size_byte = fields.Str(
        metadata={"description": "The free memory, in bytes"})
    max_mem_alloc_size_byte = fields.Str(
        metadata={"description": "The total allocatable memory, in bytes"})
    memory_ecc_state = fields.Str(
        metadata={"description": "The state of memory ecc"})
    number_of_memory_channels = fields.Str(
        metadata={"description": "Number of memory channels"})
    memory_bus_width = fields.Str(metadata={"description": "Memory bus width"})
    max_hardware_contexts = fields.Str(
        metadata={"description": "Maximum number of logical hardware contexts"})
    max_command_queue_priority = fields.Str(metadata={
                                            "description": "Maximum priority for command queues. Higher value is higher priority"})
    number_of_eus = fields.Str(
        metadata={"description": "The number of EUs"})
    number_of_tiles = fields.Str(
        metadata={"description": "The number of tiles"})
    number_of_slices = fields.Str(
        metadata={"description": "Maximum number of slices"})
    number_of_sub_slices_per_slice = fields.Str(
        metadata={"description": "Maximum number of sub-slices per slice"})
    number_of_eus_per_sub_slice = fields.Str(
        metadata={"description": "Maximum number of EUs per sub-slice"})
    number_of_threads_per_eu = fields.Str(
        metadata={"description": "Maximum number of threads per EU"})
    physical_eu_simd_width = fields.Str(
        metadata={"description": "The physical EU simd width"})
    number_of_media_engines = fields.Str(
        metadata={"description": "The number of media engines"})
    number_of_media_enh_engines = fields.Str(
        metadata={"description": "The number of media enhancement engines"})
    gfx_data_firmware_version = fields.Str(
        metadata={"description": "The GFX_DATA firmware version of device"})
    gfx_data_firmware_name = fields.Str(
        metadata={"description": "The GFX_DATA firmware name of device"})
    amc_firmware_version = fields.Str(
        metadata={"description": "The AMC firmware version of device"})
    amc_firmware_name = fields.Str(
        metadata={"description": "The AMC firmware name of device"})
    gfx_pscbin_firmware_version = fields.Str(
        metadata={"description": "The PSC firmware version of device"})
    gfx_pscbin_firmware_name = fields.Str(
        metadata={"description": "The PSC firmware name of device"})
    gfx_firmware_status = fields.Str(
        metadata={"description": "The GFX firmware status"})
    health = fields.Nested(DevicePropertiesLinkSchema)
    topology = fields.Nested(DevicePropertiesLinkSchema)


def get_device_properties(deviceId):
    """
    Get device detail properties.
    ---
    get:
        tags:
            - "Devices"
        description: Get device properties
        parameters:
            - 
                name: deviceId
                in: path
                description: Device id
                type: integer
            - 
                name: Get device properties request schema
                in: body
                description: Credential used for redfish auth
                schema: RequestAMCFwVersionSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: DevicePropertiesSchema
            400:
                description: Error
            404:
                description: "Device not found"
            500:
                description: Error
    """
    try:
        if request.data:
            req = request.get_json()
            try:
                RequestAMCFwVersionSchema().load(req)
            except ValidationError as err:
                key = next(iter(err.messages))
                value = err.messages[key]
                errStr = key+": "+";".join(value)
                return jsonify({'error': errStr}), 400
            code, message, data = stub.getDeviceProperties(
                int(deviceId), req.get("username", ""), req.get("password", ""))
        else:
            code, message, data = stub.getDeviceProperties(int(deviceId))
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        if message == "Device not found":
            return jsonify(error), 404
        return jsonify(error), 400
    except Exception as e:
        print(e)
        return "Internal error", 500


class RequestAMCFwVersionSchema(Schema):
    username = fields.Str(
        metadata={"description": "Username for redfish auth"})
    password = fields.Str(
        metadata={"description": "Password for redfish auth"})


class AMCFwVersionSchema(Schema):
    amc_fw_version = fields.List(
        fields.Str(metadata={"description": "AMC versions"}))
    error = fields.Str(metadata={"description": "Error message"})


def get_amc_fw_versions():
    """
    Get amc firmware versions.
    ---
    get:
        tags:
            - "Devices"
        description: Get amc firmware versions.
        consumes:
            - application/json
        parameters:
            - 
                name: Get AMC firmware version request schema
                in: body
                description: Credential used for redfish auth
                schema: RequestAMCFwVersionSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: AMCFwVersionSchema
            500:
                description: Error
    """
    if request.data:
        req = request.get_json()
        try:
            RequestAMCFwVersionSchema().load(req)
        except ValidationError as err:
            key = next(iter(err.messages))
            value = err.messages[key]
            errStr = key+": "+";".join(value)
            return jsonify({'error': errStr}), 400
        code, message, data = stub.getAMCFirmwareVersions(
            req.get("username", ""), req.get("password", ""))
    else:
        code, message, data = stub.getAMCFirmwareVersions("", "")
    if code != 0:
        return jsonify({"error": message}), 500
    return jsonify(data)
