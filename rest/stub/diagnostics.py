import core_pb2
from .grpc_stub import stub

diagnosticTypeEnumToString = {
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_ENV_VARIABLES: "Env Variables",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_LIBRARY: "Library",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_PERMISSION: "Permission",
    core_pb2.DiagnosticsComponentInfo.DIAG_SOFTWARE_EXCLUSIVE: "Exclusive",
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
    resp = stub.runDiagnostics(
        core_pb2.RunDiagnosticsRequest(deviceId=deviceId, level=level))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    return 0, "OK", {"result": "OK"}


def runDiagnosticsByGroup(groupId, level):
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
    data['deviceId'] = deviceId
    data['level'] = resp.level
    data['finished'] = resp.finished
    data['message'] = resp.message
    data['component_count'] = resp.count
    componentList = []
    i = 0
    for component in resp.componentInfo:
        if i >= resp.count:
            break
        i = i + 1
        new_component = dict()
        new_component['type'] = diagnosticTypeEnumToString[component.type]
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
    for diagTaskInfo in resp.taskInfo:
        data = dict()
        data['deviceId'] = diagTaskInfo.deviceId
        data['level'] = diagTaskInfo.level
        data['finished'] = diagTaskInfo.finished
        data['message'] = diagTaskInfo.message
        data['component_count'] = diagTaskInfo.count

        componentList = []
        i = 0
        for component in diagTaskInfo.componentInfo:
            if i >= diagTaskInfo.count:
                break
            i = i + 1
            new_component = dict()
            new_component['type'] = diagnosticTypeEnumToString[component.type]
            new_component['finished'] = component.finished
            new_component['result'] = diagnosticResultEnumToString[component.result]
            new_component['message'] = component.message
            componentList.append(new_component)
        data['component_list'] = componentList
        datas.append(data)

    return 0, "OK", dict(group_id=groupId, device_count=len(datas), device_list=datas)
