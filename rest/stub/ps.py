#
# Copyright (C) 2022-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file ps.py
#

from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2


def getDeviceUtilByProc(deviceId):
    resp = stub.getDeviceUtilizationByProcess(
            core_pb2.DeviceUtilizationByProcessRequest(
                deviceId = deviceId, 
                utilizationInterval = 200 * 1000))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = []
    for d in resp.processList:
        util = dict()
        util['process_id'] = d.processId
        util['process_name'] = d.processName
        util['device_id'] = d.deviceId
        util['mem_size'] = d.memSize
        util['shared_mem_size'] = d.sharedMemSize
        data.append(util)
    return 0, "OK", data 

def getAllDeviceUtilByProc():
    resp = stub.getAllDeviceUtilizationByProcess(
            core_pb2.UtilizationInterval(
                utilInterval = 200 * 1000))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = []
    for d in resp.processList:
        util = dict()
        util['process_id'] = d.processId
        util['process_name'] = d.processName
        util['device_id'] = d.deviceId
        util['mem_size'] = d.memSize
        util['shared_mem_size'] = d.sharedMemSize
        data.append(util)
    return 0, "OK", data 
