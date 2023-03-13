#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file dump_raw_data.py
#

from google.protobuf import empty_pb2
import core_pb2
from .grpc_stub import stub
from .xpum_enums import XpumDumpType
import xpum_logger as logger

url_prefix = "/download"

dump_folder = "/tmp/xpumdump"


def startDumpRawDataTask(deviceId, tileId, metricsTypeList):

    enumList = [core_pb2.GeneralEnum(
        value=XpumDumpType[m].value) for m in metricsTypeList]

    resp = stub.startDumpRawDataTask(core_pb2.StartDumpRawDataTaskRequest(
        deviceId=deviceId,
        tileId=tileId,
        metricsTypeList=enumList
    ))

    if len(resp.errorMsg) != 0:
        logger.audit("Dump", "Failed",
                     "Start dump raw data task on device {} tile {}", deviceId, tileId)
        return resp.errorNo, resp.errorMsg, None

    taskInfo = resp.taskInfo

    data = dict()

    data["task_id"] = taskInfo.dumpTaskId
    data["dump_file_path"] = taskInfo.dumpFilePath.replace(
        dump_folder, url_prefix, 1)

    logger.audit("Dump", "Succeed",
                 "Start dump raw data task {} on device {} tile {}, filename: {}", data["task_id"], deviceId, tileId, data["dump_file_path"])

    return 0, "OK", data


def stopDumpRawDataTask(taskId):
    resp = stub.stopDumpRawDataTask(
        core_pb2.StopDumpRawDataTaskRequest(dumpTaskId=taskId))
    if len(resp.errorMsg) != 0:
        logger.audit("Dump", "Failed",
                     "Stop dump raw data task {}", taskId)
        return resp.errorNo, resp.errorMsg, None
    taskInfo = resp.taskInfo
    data = dict()

    data["task_id"] = taskInfo.dumpTaskId
    data["dump_file_path"] = taskInfo.dumpFilePath.replace(
        dump_folder, url_prefix, 1)

    logger.audit("Dump", "Succeed",
                 "Stop dump raw data task {}", taskId)

    return 0, "OK", data


def listDumpRawDataTasks():
    resp = stub.listDumpRawDataTasks(empty_pb2.Empty())

    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None

    data = dict(dump_task_ids=[
                taskInfo.dumpTaskId for taskInfo in resp.taskList])

    return 0, "OK", data
