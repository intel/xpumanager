from google.protobuf import empty_pb2
import core_pb2
from .grpc_stub import stub


def startDumpRawDataTask(deviceId, tileId, metricsTypeList):

    resp = stub.startDumpRawDataTask(core_pb2.StartDumpRawDataTaskRequest(
        deviceId,
        tileId,
        metricsTypeList
    ))

    if len(resp.errorMsg) != 0:
        return resp.status, resp.errorMsg, None

    taskInfo = resp.taskinfo

    data = dict()

    data["task_id"] = taskInfo.dumptaskid
    data["dump_file_path"] = taskInfo.dumpfilepath

    return 0, "OK", data


def stopDumpRawDataTask(taskId):
    resp = stub.stopDumpRawDataTask(
        core_pb2.StopDumpRawDataTaskRequest(taskId))
    if len(resp.errorMsg) != 0:
        return resp.status, resp.errorMsg, None
    taskInfo = resp.taskinfo
    data = dict()

    data["task_id"] = taskInfo.dumptaskid
    data["dump_file_path"] = taskInfo.dumpfilepath

    return 0, "OK", data


def listDumpRawDataTasks():
    resp = stub.listDumpRawDataTasks(empty_pb2.Empty())

    if len(resp.errorMsg) != 0:
        return resp.status, resp.errorMsg, None

    data = dict(dump_task_ids=[
                taskInfo.dumptaskid for taskInfo in resp.tasklist])

    return 0, "OK", data
