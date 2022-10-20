#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file agent_settings.py
#

from google.protobuf import empty_pb2
import core_pb2
from .grpc_stub import stub


def setAgentConfig(configList):
    configEntries = []
    for d in configList:
        flexTypeValue = core_pb2.FlexTypeValue()
        valueType = d["valueType"]
        value = d["value"]
        key = d["name"]
        if valueType == "int64":
            flexTypeValue.intValue = value
        elif valueType == "double":
            flexTypeValue.floatValue = value
        elif valueType == "string":
            flexTypeValue.stringValue = value
        entry = core_pb2.AgentConfigEntry(key=key, value=flexTypeValue)
        configEntries.append(entry)
    resp = stub.setAgentConfig(
        core_pb2.SetAgentConfigRequest(configEntries=configEntries))
    if len(resp.errorList) != 0:
        errorList = [dict(name=e.key, error_msg=[e.errorMsg])
                     for e in resp.errorList]
        return 1, "Fail to set some fields", errorList
    configEntries = []
    for entry in resp.entryList.configEntries:
        vt = entry.value.WhichOneof("value")
        if vt == "intValue":
            value = entry.value.intValue
        elif vt == "floatValue":
            value = entry.value.floatValue
        elif vt == "stringValue":
            value = entry.value.stringValue
        configEntries.append(dict(name=entry.key, value=value))
    return 0, "OK", configEntries


def getAllAgentConfig():
    resp = stub.getAgentConfig(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None
    configEntries = []
    for entry in resp.entryList.configEntries:
        vt = entry.value.WhichOneof("value")
        if vt == "intValue":
            value = entry.value.intValue
        elif vt == "floatValue":
            value = entry.value.floatValue
        elif vt == "stringValue":
            value = entry.value.stringValue
        configEntries.append(dict(name=entry.key, value=value))
    return 0, "OK", configEntries
