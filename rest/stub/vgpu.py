#
# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file vgpu.py
#

from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2


def doVgpuPrecheck():
    resp = stub.doVgpuPrecheck(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["vmx_flag"] = "Pass" if resp.vmxFlag else "Fail"
    data["vmx_message"] = resp.vmxMessage
    data["iommu_status"] = "Pass" if resp.iommuStatus else "Fail"
    data["iommu_message"] = resp.iommuMessage
    data["sriov_status"] = "Pass" if resp.sriovStatus else "Fail"
    data["sriov_message"] = resp.sriovMessage
    return 0, "OK", data
