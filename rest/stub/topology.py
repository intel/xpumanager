#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file topology.py
#

from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2


def getTopology(deviceId):
    resp = stub.getTopology(core_pb2.DeviceId(id=deviceId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["device_id"] = deviceId
    data["affinity_localcpulist"] = resp.cpuAffinity.localCpuList
    data["affinity_localcpus"] = resp.cpuAffinity.localCpus
    data["switch_count"] = resp.switchCount
    if resp.switchCount > 0:
        data["switch_list"] = [s.switchDevicePath for s in resp.switchInfo]
    return 0, "OK", data


def getTopoXelink():
    resp = stub.getXelinkTopology(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None

    data = []
    for xelink in resp.topoInfo:
        t = dict()
        t["local_device_id"] = xelink.localDevice.deviceId
        t["local_on_subdevice"] = xelink.localDevice.onSubdevice
        t["local_subdevice_id"] = xelink.localDevice.subdeviceId
        t["local_numa_index"] = xelink.localDevice.numaIndex
        t["local_cpu_affinity"] = xelink.localDevice.cpuAffinity
        t["remote_device_id"] = xelink.remoteDevice.deviceId
        t["remote_subdevice_id"] = xelink.remoteDevice.subdeviceId
        t["link_type"] = xelink.linkType
        if len(xelink.linkPortList) > 0:
            t["port_list"] = [port for port in xelink.linkPortList]

        data.append(t)
    return 0, "OK", data


def exportTopology():
    try:
        resp = stub.getTopoXMLBuffer(empty_pb2.Empty())
        if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None
        data = dict()
        data["length"] = resp.length
        data["xmlstring"] = resp.xmlString
        return 0, "OK", data
    except Exception as ex:
        template = "An exception of type {0} occurred. Arguments:\n{1!r}"
        message = template.format(type(ex).__name__, ex.args)
        return 1, message, None
