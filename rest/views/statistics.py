from flask import jsonify
import stub
from marshmallow import Schema, fields


class StatisticsDataSchema(Schema):
    metrics_type = fields.Str(metadata={"description": "The metric type"})
    value = fields.Int(metadata={"description": "The current value"})
    avg = fields.Int(
        metadata={"description": "The average value since last query"})
    max = fields.Int(
        metadata={"description": "The max value since last query"})
    min = fields.Int(
        metadata={"description": "The min value since last query"})


class TileStatisticsSchema(Schema):
    tile_id = fields.Int(
        metadata={"description": "The tile this data belongs to"})
    data_list = fields.Nested(StatisticsDataSchema, many=True, metadata={
                              "description": "Data array"})


class StatisticsSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    begin = fields.Str(metadata={"description": "The time of last query"})
    end = fields.Str(metadata={"description": "The time of this query"})
    device_level = fields.Nested(StatisticsDataSchema, many=True, metadata={
                                 "description": "Device level statistics datas"})
    tile_level = fields.Nested(TileStatisticsSchema, many=True, metadata={
                               "description": "Tile level statistics datas"})


def get_statistics(deviceId):
    """
    Get statistics by device
    ---
    get:
        tags:
            - "Statistics"
        description: Get statistics by device
        parameters:
            - 
                name: deviceId
                in: path
                description: Device id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: StatisticsSchema
                examples: 
                    application/json:
                        { "begin": "2021-10-25T10:25:44.000Z", "device_id": 0, "device_level": [ { "avg": 23, "max": 25, "metrics_type": "XPUM_STATS_POWER", "min": 23, "value": 23 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 20948278 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_RESET", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE", "value": 0 } ], "end": "2021-10-25T10:40:22.095Z", "tile_level": [ { "data_list": [ { "avg": 1150, "max": 1150, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 1150, "value": 1150 }, { "avg": 35, "max": 35, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 35, "value": 35 }, { "avg": 134410240, "max": 134443008, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 134410240, "value": 134410240 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_UTILIZATION", "min": 0, "value": 0 }, { "avg": 10, "max": 10, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 10, "value": 10 }, { "metrics_type": "XPUM_STATS_MEMORY_READ", "value": 5412884 }, { "metrics_type": "XPUM_STATS_MEMORY_WRITE", "value": 5430895 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_ACTIVE", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_STALL", "min": 0, "value": 0 }, { "avg": 100, "max": 100, "metrics_type": "XPUM_STATS_EU_IDLE", "min": 100, "value": 100 }, { "avg": 1300, "max": 1300, "metrics_type": "XPUM_STATS_GPU_REQUEST_FREQUENCY", "min": 1300, "value": 1300 } ], "tile_id": 0 }, { "data_list": [ { "avg": 300, "max": 300, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 300, "value": 300 }, { "avg": 34, "max": 35, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 34, "value": 35 }, { "avg": 134344704, "max": 134361088, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 134344704, "value": 134344704 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_UTILIZATION", "min": 0, "value": 0 }, { "avg": 10, "max": 10, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 10, "value": 10 }, { "metrics_type": "XPUM_STATS_MEMORY_READ", "value": 5414234 }, { "metrics_type": "XPUM_STATS_MEMORY_WRITE", "value": 5416830 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_ACTIVE", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_STALL", "min": 0, "value": 0 }, { "avg": 100, "max": 100, "metrics_type": "XPUM_STATS_EU_IDLE", "min": 100, "value": 100 }, { "avg": 300, "max": 300, "metrics_type": "XPUM_STATS_GPU_REQUEST_FREQUENCY", "min": 300, "value": 300 } ], "tile_id": 1 } ] }
            500:
                description: Error
    """
    code, message, data = stub.getStatisticsNotForPrometheus(deviceId)
    if code == 0:
        return jsonify(data)
    elif code == stub.XpumResult["XPUM_RESULT_DEVICE_NOT_FOUND"].value:
        error = dict(message=message)
        return jsonify(error), 400
    else:
        error = dict(message="Error code: {}, error message: {}".format(
            stub.XpumResult(code).name, message))
        return jsonify(error), 500

class GroupStatisticsSchema(Schema):
    group_id = fields.Int(metadata={"description": "Group id"})
    datas = fields.Nested(StatisticsSchema, many=True, metadata={
                          "description": "Statistics data for devices in group"})


def get_group_statistics(groupId):
    """Get statistics by group
    ---
    get:
        tags:
            - "Statistics"
        description: Get statistics by group
        parameters:
            - 
                name: groupId
                in: path
                description: Group id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: GroupStatisticsSchema
                examples:
                    application/json:
                        { "datas": [ { "begin": "2021-10-25T16:07:32.000Z", "device_id": 0, "device_level": [ { "avg": 23, "max": 24, "metrics_type": "XPUM_STATS_POWER", "min": 23, "value": 23 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 2609420 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_RESET", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE", "value": 0 } ], "end": "2021-10-25T16:09:21.020Z", "tile_level": [ { "data_list": [ { "avg": 1150, "max": 1150, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 1150, "value": 1150 }, { "avg": 36, "max": 36, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 36, "value": 36 }, { "avg": 134410061, "max": 134410240, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 134393856, "value": 134410240 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_UTILIZATION", "min": 0, "value": 0 }, { "avg": 10, "max": 10, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 10, "value": 10 }, { "metrics_type": "XPUM_STATS_MEMORY_READ", "value": 674133 }, { "metrics_type": "XPUM_STATS_MEMORY_WRITE", "value": 681828 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_ACTIVE", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_STALL", "min": 0, "value": 0 }, { "avg": 100, "max": 100, "metrics_type": "XPUM_STATS_EU_IDLE", "min": 100, "value": 100 }, { "avg": 1300, "max": 1300, "metrics_type": "XPUM_STATS_GPU_REQUEST_FREQUENCY", "min": 1300, "value": 1300 } ], "tile_id": 0 }, { "data_list": [ { "avg": 300, "max": 300, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 300, "value": 300 }, { "avg": 36, "max": 36, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 36, "value": 36 }, { "avg": 134344704, "max": 134344704, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 134344704, "value": 134344704 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_UTILIZATION", "min": 0, "value": 0 }, { "avg": 10, "max": 10, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 10, "value": 10 }, { "metrics_type": "XPUM_STATS_MEMORY_READ", "value": 676135 }, { "metrics_type": "XPUM_STATS_MEMORY_WRITE", "value": 680448 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_ACTIVE", "min": 0, "value": 0 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_EU_STALL", "min": 0, "value": 0 }, { "avg": 100, "max": 100, "metrics_type": "XPUM_STATS_EU_IDLE", "min": 100, "value": 100 }, { "avg": 300, "max": 300, "metrics_type": "XPUM_STATS_GPU_REQUEST_FREQUENCY", "min": 300, "value": 300 } ], "tile_id": 1 } ] } ], "group_id": 1 }

            400:
                description: Error
            500:
                description: Error
    """
    code, message, data = stub.getStatisticsByGroupNotForPrometheus(groupId)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), 500
    return jsonify(data)
