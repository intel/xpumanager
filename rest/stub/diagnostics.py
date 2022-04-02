#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file diagnostics.py
#

import core_pb2
from .grpc_stub import stub
import datetime
import xpum_logger as logger

diagnosticTypeEnumToString = {
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_ENV_VARIABLES: "XPUM_DIAG_SOFTWARE_ENV_VARIABLES",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_LIBRARY: "XPUM_DIAG_SOFTWARE_LIBRARY",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_PERMISSION: "XPUM_DIAG_SOFTWARE_PERMISSION",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_EXCLUSIVE: "XPUM_DIAG_SOFTWARE_EXCLUSIVE",
    core_pb2.DiagnosticsComponentInfo.DIAG_HARDWARE_SYSMAN: "XPUM_DIAG_HARDWARE_SYSMAN",
    core_pb2.DiagnosticsComponentInfo.DIAG_INTEGRATION_PCIE: "XPUM_DIAG_INTEGRATION_PCIE",
    core_pb2.DiagnosticsComponentInfo.DIAG_MEDIA_CODEC: "XPUM_DIAG_MEDIA_CODEC",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_COMPUTATION: "XPUM_DIAG_PERFORMANCE_COMPUTATION",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_POWER: "XPUM_DIAG_PERFORMANCE_POWER",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_MEMORY_BANDWIDTH: "XPUM_DIAG_PERFORMANCE_MEMORY_BANDWIDTH",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_MEMORY_ALLOCATION: "XPUM_DIAG_PERFORMANCE_MEMORY_ALLOCATION"
}

diagnosticResultEnumToString = {
    core_pb2.DIAG_RESULT_UNKNOWN: "Unknown",
    core_pb2.DIAG_RESULT_PASS: "Pass",
    core_pb2.DIAG_RESULT_FAIL: "Fail"
}


def runDiagnostics(deviceId, level):
    if level not in [1, 2, 3]:
        return 1, "invalid level", None
    resp = stub.runDiagnostics(
        core_pb2.RunDiagnosticsRequest(deviceId=deviceId, level=level))
    if len(resp.errorMsg) != 0:
        logger.audit("Diagnostics", "Failed",
                     "Failed to run level-{} diagnostics on device {}", level, deviceId)
        return 1, resp.errorMsg, None
    logger.audit("Diagnostics", "Succeed",
                 "Succeed to run level-{} diagnostics on device {}", level, deviceId)
    return 0, "OK", {"result": "OK"}


def runDiagnosticsByGroup(groupId, level):
    if level not in [1, 2, 3]:
        return 1, "invalid level", None
    resp = stub.runDiagnosticsByGroup(
        core_pb2.RunDiagnosticsByGroupRequest(groupId=groupId, level=level))
    if len(resp.errorMsg) != 0:
        logger.audit("Diagnostics", "Failed",
                     "Failed to run level-{} diagnostics on group {}", level, groupId)
        return 1, resp.errorMsg, None
    logger.audit("Diagnostics", "Succeed",
                 "Succeed to run level-{} diagnostics on group {}", level, groupId)
    return 0, "OK", {"result": "OK"}


def getDiagnosticsResult(deviceId):
    resp = stub.getDiagnosticsResult(core_pb2.DeviceId(id=deviceId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None

    data = dict()
    data['device_id'] = deviceId
    data['level'] = resp.level
    data['finished'] = resp.finished
    data['result'] = diagnosticResultEnumToString[resp.result]
    data['message'] = resp.message
    data['component_count'] = resp.count
    beginTimestamp = datetime.datetime.fromtimestamp(
        resp.startTime/1e3, datetime.timezone.utc)
    data['start_time'] = beginTimestamp.isoformat(
        timespec='milliseconds').replace('+00:00', 'Z')
    if resp.finished:
        endTimestamp = datetime.datetime.fromtimestamp(
            resp.endTime/1e3, datetime.timezone.utc)
        data['end_time'] = endTimestamp.isoformat(
            timespec='milliseconds').replace('+00:00', 'Z')
    componentList = []
    i = 0
    for component in resp.componentInfo:
        if i >= resp.count:
            break
        i = i + 1
        new_component = dict()
        new_component['component_type'] = diagnosticTypeEnumToString[component.type]
        new_component['finished'] = component.finished
        new_component['result'] = diagnosticResultEnumToString[component.result]
        new_component['message'] = component.message
        if component.type == core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_EXCLUSIVE and component.result == core_pb2.DIAG_RESULT_FAIL:
            process_list_resp = stub.getDeviceProcessState(
                core_pb2.DeviceId(id=deviceId))
            if len(resp.errorMsg) == 0:
                processList = []
                for process in process_list_resp.processlist:
                    new_process = dict()
                    new_process["process_id"] = process.processId
                    new_process["process_name"] = process.processName
                    processList.append(new_process)
                new_component['process_list'] = processList
        componentList.append(new_component)
    data['component_list'] = componentList

    return 0, "OK", data


def getDiagnosticsResultByGroup(groupId):
    resp = stub.getDiagnosticsResultByGroup(core_pb2.GroupId(id=groupId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None

    datas = []
    finished = True
    for diagTaskInfo in resp.taskInfo:
        data = dict()
        data['device_id'] = diagTaskInfo.deviceId
        data['level'] = diagTaskInfo.level
        data['finished'] = diagTaskInfo.finished
        finished = finished & diagTaskInfo.finished
        data['result'] = diagnosticResultEnumToString[diagTaskInfo.result]
        data['message'] = diagTaskInfo.message
        data['component_count'] = diagTaskInfo.count
        beginTimestamp = datetime.datetime.fromtimestamp(
            diagTaskInfo.startTime/1e3, datetime.timezone.utc)
        data['start_time'] = beginTimestamp.isoformat(
            timespec='milliseconds').replace('+00:00', 'Z')
        if diagTaskInfo.finished:
            endTimestamp = datetime.datetime.fromtimestamp(
                diagTaskInfo.endTime/1e3, datetime.timezone.utc)
            data['end_time'] = endTimestamp.isoformat(
                timespec='milliseconds').replace('+00:00', 'Z')
        componentList = []
        i = 0
        for component in diagTaskInfo.componentInfo:
            if i >= diagTaskInfo.count:
                break
            i = i + 1
            new_component = dict()
            new_component['component_type'] = diagnosticTypeEnumToString[component.type]
            new_component['finished'] = component.finished
            new_component['result'] = diagnosticResultEnumToString[component.result]
            new_component['message'] = component.message
            if component.type == core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_EXCLUSIVE and component.result == core_pb2.DIAG_RESULT_FAIL:
                process_list_resp = stub.getDeviceProcessState(
                    core_pb2.DeviceId(id=diagTaskInfo.deviceId))
                if len(resp.errorMsg) == 0:
                    processList = []
                    for process in process_list_resp.processlist:
                        new_process = dict()
                        new_process["process_id"] = process.processId
                        new_process["process_name"] = process.processName
                        processList.append(new_process)
                    new_component['process_list'] = processList
            componentList.append(new_component)
        data['component_list'] = componentList
        datas.append(data)

    return 0, "OK", dict(group_id=groupId, finished=finished, device_count=len(datas), device_list=datas)
