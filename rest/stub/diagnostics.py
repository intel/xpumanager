import core_pb2
from .grpc_stub import stub
import datetime

diagnosticTypeEnumToString = {
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_ENV_VARIABLES: "Software Env Variables",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_LIBRARY: "Software Library",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_PERMISSION: "Software Permission",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_EXCLUSIVE: "Software Exclusive",
    core_pb2.DiagnosticsComponentInfo.DIAG_HARDWARE_SYSMAN: "Hardware Sysman",
    core_pb2.DiagnosticsComponentInfo.DIAG_INTEGRATION_PCIE: "Integration PCIe",
    core_pb2.DiagnosticsComponentInfo.DIAG_MEDIA_CODEC: "Media Codec",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_COMPUTE: "Performance Computation",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_POWER: "Performance Power",
    core_pb2.DiagnosticsComponentInfo.DIAG_PERFORMANCE_MEMORY: "Performance Memory"
}

diagnosticResultEnumToString = {
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_UNKNOWN: "Unknown",
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_PASS: "Pass",
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_WARNING: "Warning",
    core_pb2.DiagnosticsComponentInfo.DIAG_RESULT_CRITICAL: "Critical"
}


def runDiagnostics(deviceId, level):
    if level not in [1, 2, 3]:
        return 1, "invalid level", None
    resp = stub.runDiagnostics(
        core_pb2.RunDiagnosticsRequest(deviceId=deviceId, level=level))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}


def runDiagnosticsByGroup(groupId, level):
    if level not in [1, 2, 3]:
        return 1, "invalid level", None
    resp = stub.runDiagnosticsByGroup(
        core_pb2.RunDiagnosticsByGroupRequest(groupId=groupId, level=level))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}


def getDiagnosticsResult(deviceId):
    resp = stub.getDiagnosticsResult(core_pb2.DeviceId(id=deviceId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None

    data = dict()
    data['device_id'] = deviceId
    data['level'] = resp.level
    data['finished'] = resp.finished
    data['message'] = resp.message
    data['component_count'] = resp.count
    beginTimestamp = datetime.datetime.fromtimestamp(resp.startTime/1e3)
    data['start_time'] = beginTimestamp.isoformat(timespec='milliseconds')+"Z"
    if resp.finished:
        endTimestamp = datetime.datetime.fromtimestamp(resp.endTime/1e3)
        data['end_time'] = endTimestamp.isoformat(timespec='milliseconds')+"Z"
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
        data['message'] = diagTaskInfo.message
        data['component_count'] = diagTaskInfo.count
        beginTimestamp = datetime.datetime.fromtimestamp(diagTaskInfo.startTime/1e3)
        data['start_time'] = beginTimestamp.isoformat(timespec='milliseconds')+"Z"
        if diagTaskInfo.finished:
            endTimestamp = datetime.datetime.fromtimestamp(diagTaskInfo.endTime/1e3)
            data['end_time'] = endTimestamp.isoformat(timespec='milliseconds')+"Z"
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
            componentList.append(new_component)
        data['component_list'] = componentList
        datas.append(data)

    return 0, "OK", dict(group_id=groupId, finished=finished, device_count=len(datas), device_list=datas)
