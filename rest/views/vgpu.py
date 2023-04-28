#
# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file vgpu.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields, validate, ValidationError


class VgpuPrecheckResultSchema(Schema):
    vmx_flag = fields.String(metadata={"description": "VMX Flag Check"})
    vmx_message = fields.String(metadata={"description": "VMX flag message"})
    iommu_status = fields.String(metadata={"description": "IOMMU status"})
    iommu_message = fields.String(metadata={"description": "IOMMU message"})
    stiov_status = fields.String(metadata={"description": "SR-IOV status"})
    sriov_message = fields.String(metadata={"description": "SR-IOV message"})

class CreateVgpuSchema(Schema):
    device_id = fields.Int(
        required=True,
        strict=True,
        validate=validate.Range(0),
        metadata={"description": "The device to create VF"}
    )
    num_vfs = fields.Int(
        required=True,
        strict=True,
        validate=validate.Range(0),
        metadata={"description": "The tile to dump raw data"}
    )
    lmem_per_vf = fields.Int(
        required=True,
        strict=True,
        validate=validate.Range(0),
        metadata={"description": "Local memory size per VF"}
    )
    # metrics_type_list = fields.List(
    #     fields.String(
    #         strict=True,
    #         validate=validate.OneOf(allow_dump_metrics),
    #         metadata={
    #             "description": "The metrics type to dump, options are:\n"+"\n".join(allow_dump_metrics)}
    #     ),
    #     required=True,
    #     validate=[validate.Length(1), is_unique]
    # )
   

def doVgpuPrecheck():
    """
    Check if BIOS settings are ready to create virtual GPUs.
    ---
    get:
        tags:
            - "vgpu"
        description: Check if BIOS settings are ready to create virtual GPUs
        responses:
            200:
                description: OK
                schema: VgpuPrecheckResultSchema
            500:
                description: Error
    """
    code, message, data = stub.doVgpuPrecheck()
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400

def createVf():
    reqData = request.get_json()
    try:
        CreateVgpuSchema().load(reqData)
    except ValidationError as err:
        return jsonify(err.messages), 400
    deviceId = reqData.get("device_id")
    numVfs = reqData.get("num_vfs")
    lmemPerVf = reqData.get("lmem_per_vf")

    code, message, data = stub.createVf(deviceId, numVfs, lmemPerVf)
    if code == 0:
        return jsonify(data)
    error = dict(Status=code, Message=message)
    return jsonify(error), 400