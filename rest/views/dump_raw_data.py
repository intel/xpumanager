#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file dump_raw_data.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields, validate, ValidationError


allow_dump_metrics = [e.name for e in stub.XpumDumpType]


def is_unique(values):
    if len(set(values)) != len(values):
        raise ValidationError("Duplicated metrics type")


class StartDumpRawDataTaskSchema(Schema):
    device_id = fields.Int(
        required=True,
        strict=True,
        validate=validate.Range(0),
        metadata={"description": "The device to dump raw data"}
    )
    tile_id = fields.Int(
        required=False,
        strict=True,
        validate=validate.Range(0),
        metadata={"description": "The tile to dump raw data"}
    )
    metrics_type_list = fields.List(
        fields.String(
            strict=True,
            validate=validate.OneOf(allow_dump_metrics),
            metadata={
                "description": "The metrics type to dump, options are:\n"+"\n".join(allow_dump_metrics)}
        ),
        required=True,
        validate=[validate.Length(1), is_unique]
    )


class DumpRawDataTaskInfoSchema(Schema):
    task_id = fields.Int(metadata={"description": "The task id"})
    dump_file_path = fields.Str(
        metadata={"description": "The path to file of dumped data"})


class ListAllDumpRawDataTaskSchema(Schema):
    dump_task_ids = fields.List(fields.Int(
        metadata={"description": "The id list of all tasks"}))


def startDumpRawDataTask():
    """
    Start dump raw data task
    ---
    post:
        tags:
            - "Dump Raw Data"
        description: Start dump raw data task
        consumes:
            - application/json
        parameters:
            -
                name: info used to create task
                in: body
                description: the information needed to create a dump raw data task
                schema: StartDumpRawDataTaskSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: DumpRawDataTaskInfoSchema
            400:
                description: Bad Request
            500:
                description: Internal Error
    """
    reqData = request.get_json()
    try:
        StartDumpRawDataTaskSchema().load(reqData)
    except ValidationError as err:
        return jsonify(err.messages), 400
    deviceId = reqData.get("device_id")
    metricsTypeList = reqData.get("metrics_type_list")
    tileId = reqData.get("tile_id", -1)
    code, message, data = stub.startDumpRawDataTask(
        deviceId, tileId, metricsTypeList)
    if code == 0:
        return jsonify(data)
    elif code == stub.XpumResult["XPUM_RESULT_DEVICE_NOT_FOUND"].value:
        error = dict(
            message="device_id {} corresponding device not found".format(deviceId))
        return jsonify(error), 400
    elif code == stub.XpumResult["XPUM_RESULT_TILE_NOT_FOUND"].value:
        error = dict(
            message="tile_id {} corresponding tile not found".format(tileId))
        return jsonify(error), 400
    else:
        error = dict(message="Error code: {}, error message: {}".format(
            stub.XpumResult(code).name, message))
        return jsonify(error), 500


def stopDumpRawDataTask(taskId):
    """
    Stop dump raw data task
    ---
    delete:
        tags:
            - "Dump Raw Data"
        description: Stop dump raw data task
        parameters:
            - 
                name: taskId
                in: path
                description: the dump raw data task id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: DumpRawDataTaskInfoSchema
            404:
                description: Task not found
            500:
                description: Internal Error
    """
    code, message, data = stub.stopDumpRawDataTask(taskId)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), 500
    return jsonify(data)


def listDumpRawDataTasks():
    """
    List all dump raw data task
    ---
    get:
        tags:
            - "Dump Raw Data"
        description: List all dump raw data task
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: ListAllDumpRawDataTaskSchema
            500:
                description: Internal Error
    """
    code, message, data = stub.listDumpRawDataTasks()
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), 500
    return jsonify(data)
