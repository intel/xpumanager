from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class StartDumpRawDataTaskSchema(Schema):
    device_id = fields.Int(
        metadata={"description": "The device to dump raw data"}, required=True)
    tile_id = fields.Int(
        metadata={"description": "The tile to dump raw data"}, required=False)
    metrics_type_list = fields.Int(
        many=True,
        metadata={"description": "The metrics types to dump"}, required=True
    )


class DumpRawDataTaskInfoSchema(Schema):
    task_id = fields.Int(metadata={"description": "The task id"})
    dump_file_path = fields.Str(
        metadata={"description": "The path to file of dumped data"})


class ErrorSchema(Schema):
    status = fields.Int(metadata={"description": "The error status"})
    message = fields.Str(metadata={"description": "The error message"})


class ListAllDumpRawDataTaskSchema(Schema):
    dump_task_ids = fields.Int(
        many=True, metadata={"description": "The id list of all tasks"})

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
                schema: ErrorSchema
            500:
                description: Internal Error
    """
    reqData = request.get_json()
    deviceId = reqData.get("device_id", None)
    if deviceId is None:
        error = dict(message="Device id must be assigned", status=1)
        return jsonify(error), 400
    metricsTypeList = reqData.get("metrics_type_list", [])
    if not metricsTypeList:
        error = dict(message="`metrics_type_list` can not be empty", status=1)
        return jsonify(error), 400
    tileId = reqData.get("tile_id", -1)
    code, message, data = stub.startDumpRawDataTask(
        deviceId, tileId, metricsTypeList)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), 500
    return jsonify(data)


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
