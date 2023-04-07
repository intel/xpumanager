#
# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file vgpu.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class VgpuPrecheckResultSchema(Schema):
    vmx_flag = fields.String(metadata={"description": "VMX Flag Check"})
    vmx_message = fields.String(metadata={"description": "VMX flag message"})
    iommu_status = fields.String(metadata={"description": "IOMMU status"})
    iommu_message = fields.String(metadata={"description": "IOMMU message"})
    stiov_status = fields.String(metadata={"description": "SR-IOV status"})
    sriov_message = fields.String(metadata={"description": "SR-IOV message"})
   

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

