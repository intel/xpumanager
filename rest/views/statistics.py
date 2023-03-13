#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file statistics.py
#

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


class EngineUtilDataSchema(Schema):
    engine_id = fields.Int(
        metadata={"description": "Engine id, unique in same tile and same type"})
    value = fields.Number(metadata={"description": "The current value"})
    avg = fields.Number(
        metadata={"description": "The average value since last query"})
    min = fields.Number(
        metadata={"description": "The min value since last query"})
    max = fields.Number(
        metadata={"description": "The max value since last query"})


class EngineUtilSchema(Schema):
    compute = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "compute engine utilizations"})
    render = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "render engine utilizations"})
    decoder = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "decoder engine utilizations"})
    encoder = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "encoder engine utilizations"})
    copy = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "copy engine utilizations"})
    media_enhancement = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "media enhancement engine utilizations"})
    three_d = fields.Nested(EngineUtilDataSchema, many=True, metadata={
        "description": "3d engine utilizations"}, data_key="3d")


class FabricThroughputSchema(Schema):
    name = fields.Str(metadata={"description": "Fabric throughput name"})
    value = fields.Number(metadata={"description": "The current value"})
    avg = fields.Number(
        metadata={"description": "The average value since last query"})
    min = fields.Number(
        metadata={"description": "The min value since last query"})
    max = fields.Number(
        metadata={"description": "The max value since last query"})


class TileStatisticsSchema(Schema):
    tile_id = fields.Int(
        metadata={"description": "The tile this data belongs to"})
    data_list = fields.Nested(StatisticsDataSchema, many=True, metadata={
                              "description": "Data array"})
    engine_util = fields.Nested(EngineUtilSchema, metadata={
        "description": "Engine utilizations"})


class StatisticsSchema(Schema):
    device_id = fields.Int(metadata={"description": "Device id"})
    begin = fields.Str(metadata={"description": "The time of last query"})
    end = fields.Str(metadata={"description": "The time of this query"})
    device_level = fields.Nested(StatisticsDataSchema, many=True, metadata={
                                 "description": "Device level statistics datas"})
    tile_level = fields.Nested(TileStatisticsSchema, many=True, metadata={
                               "description": "Tile level statistics datas"})
    engine_util = fields.Nested(EngineUtilSchema, metadata={
        "description": "Engine utilizations"})
    fabric_throughput = fields.Nested(FabricThroughputSchema, many=True, metadata={
        "description": "Fabric throughput statistics"})


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
                        { "begin": "2022-02-23T05:21:08.000Z", "device_id": 0, "device_level": [ { "avg": 63.12, "max": 63.48, "metrics_type": "XPUM_STATS_POWER", "min": 63.1, "value": 63.26 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 16169744 } ], "end": "2022-02-23T05:25:23.506Z", "tile_level": [ { "data_list": [ { "avg": 24.01, "max": 24.22, "metrics_type": "XPUM_STATS_POWER", "min": 24.01, "value": 24.12 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 6159791 }, { "avg": 400, "max": 400, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 400, "value": 400 }, { "avg": 35.99, "max": 37.0, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 35.0, "value": 37.0 }, { "avg": 8501719040, "max": 8501719040, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 8501719040, "value": 8501719040 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 0, "value": 0 }, { "avg": 100.0, "max": 100.0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 100.0, "value": 100.0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_RESET", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE", "value": 0 }, { "avg": 37.22, "max": 38.0, "metrics_type": "XPUM_STATS_MEMORY_TEMPERATURE", "min": 37.0, "value": 38.0 } ], "engine_util": { "3d": [], "compute": [ { "avg": 99.76, "engine_id": 0, "max": 99.76, "min": 99.76, "value": 0.0 }, { "avg": 0.0, "engine_id": 1, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 2, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 3, "max": 0.0, "min": 0.0, "value": 0.0 } ], "copy": [ { "avg": 0.0, "engine_id": 36, "max": 0.0, "min": 0.0, "value": 0.04 } ], "decoder": [ { "avg": 0.0, "engine_id": 8, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 9, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 10, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 11, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 12, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 13, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 14, "max": 0.0, "min": 0.0, "value": 0.02 } ], "encoder": [ { "avg": 0.0, "engine_id": 22, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 23, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 24, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 25, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 26, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 27, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 28, "max": 0.0, "min": 0.0, "value": 0.03 } ], "media_enhancement": [ { "avg": 0.0, "engine_id": 38, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 39, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 40, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 41, "max": 0.0, "min": 0.0, "value": 0.05 } ], "render": [] }, "tile_id": 0 }, { "data_list": [ { "avg": 8.68, "max": 8.9, "metrics_type": "XPUM_STATS_POWER", "min": 8.67, "value": 8.78 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 2243484 }, { "avg": 158, "max": 300, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 0, "value": 300 }, { "avg": 36.07, "max": 38.0, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 36.0, "value": 37.0 }, { "avg": 148901888, "max": 148901888, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 148901888, "value": 148901888 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 0, "value": 0 }, { "avg": 0.0, "max": 0.0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 0.0, "value": 0.0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_RESET", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE", "value": 0 }, { "avg": 34.8, "max": 35.0, "metrics_type": "XPUM_STATS_MEMORY_TEMPERATURE", "min": 34.0, "value": 35.0 } ], "engine_util": { "3d": [], "compute": [ { "avg": 0.0, "engine_id": 4, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 5, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 6, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 7, "max": 0.0, "min": 0.0, "value": 0.0 } ], "copy": [ { "avg": 0.0, "engine_id": 37, "max": 0.0, "min": 0.0, "value": 0.04 } ], "decoder": [ { "avg": 0.0, "engine_id": 15, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 16, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 17, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 18, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 19, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 20, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 21, "max": 0.0, "min": 0.0, "value": 0.02 } ], "encoder": [ { "avg": 0.0, "engine_id": 29, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 30, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 31, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 32, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 33, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 34, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 35, "max": 0.0, "min": 0.0, "value": 0.03 } ], "media_enhancement": [ { "avg": 0.0, "engine_id": 42, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 43, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 44, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 45, "max": 0.0, "min": 0.0, "value": 0.05 } ], "render": [] }, "tile_id": 1 } ] }
            500:
                description: Error
    """
    code, message, data = stub.getStatisticsNotForPrometheus(deviceId)
    if code == stub.XpumResult["XPUM_RESULT_DEVICE_NOT_FOUND"].value:
        error = dict(message=message)
        return jsonify(error), 400
    elif code != 0:
        error = dict(message="Error code: {}, error message: {}".format(
            stub.XpumResult(code).name, message))
        return jsonify(error), 500
    engineCode, engineMsg, engineData = stub.getEngineStatistics(deviceId)
    if engineCode != 0:
        error = dict(message="Error code: {}, error message: {}".format(
            stub.XpumResult(engineCode).name, engineMsg))
        return jsonify(error), 500
    fabricCode, fabricMsg, fabricData = stub.getFabricStatistics(deviceId)
    if fabricCode != 0:
        error_name = stub.XpumResult(fabricCode).name
        if error_name == "XPUM_METRIC_NOT_SUPPORTED" or error_name == "XPUM_METRIC_NOT_ENABLED":
            pass
        else:
            error = dict(message="Error code: {}, error message: {}".format(
                stub.XpumResult(fabricCode).name, fabricMsg))
            return jsonify(error), 500
    # update fabric throughput data
    if fabricData is not None and "fabric_throughput" in fabricData:
        data["fabric_throughput"] = fabricData["fabric_throughput"]
    # update engine util
    if "engine_util" not in engineData:
        return jsonify(data)
    engineUtilData = engineData["engine_util"]
    if "device_level" in engineUtilData:
        data["engine_util"] = engineUtilData["device_level"]
    if "tile_level" in data:
        for t in data["tile_level"]:
            tileId = t.get("tile_id", None)
            if tileId in engineUtilData:
                t["engine_util"] = engineUtilData[tileId]
    return jsonify(data)


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
                        { "datas": [ { "begin": "2022-02-23T05:41:39.002Z", "device_id": 0, "device_level": [ { "avg": 72.09, "max": 89.65, "metrics_type": "XPUM_STATS_POWER", "min": 63.04, "value": 89.48 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 29276739 } ], "end": "2022-02-23T05:48:18.004Z", "tile_level": [ { "data_list": [ { "avg": 24.03, "max": 24.66, "metrics_type": "XPUM_STATS_POWER", "min": 24.02, "value": 24.36 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 9633618 }, { "avg": 400, "max": 400, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 400, "value": 400 }, { "avg": 36.0, "max": 40.0, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 36.0, "value": 40.0 }, { "avg": 13673419860, "max": 14983741440, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 12814774272, "value": 14983725056 }, { "avg": 0, "max": 0, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 0, "value": 0 }, { "avg": 9, "max": 1465, "metrics_type": "XPUM_STATS_MEMORY_READ_THROUGHPUT", "min": 8, "value": 11 }, { "avg": 210, "max": 98759, "metrics_type": "XPUM_STATS_MEMORY_WRITE_THROUGHPUT", "min": 7, "value": 8 }, { "avg": 100.0, "max": 100.0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 100.0, "value": 100.0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_RESET", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE", "value": 0 }, { "avg": 37.96, "max": 40.0, "metrics_type": "XPUM_STATS_MEMORY_TEMPERATURE", "min": 37.0, "value": 40.0 } ], "engine_util": { "3d": [], "compute": [ { "avg": 99.75, "engine_id": 0, "max": 100.0, "min": 99.49, "value": 0.0 }, { "avg": 0.0, "engine_id": 1, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 2, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 3, "max": 0.0, "min": 0.0, "value": 0.0 } ], "copy": [ { "avg": 0.0, "engine_id": 36, "max": 43.61, "min": 0.0, "value": 0.04 } ], "decoder": [ { "avg": 0.0, "engine_id": 8, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 9, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 10, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 11, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 12, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 13, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 14, "max": 0.0, "min": 0.0, "value": 0.02 } ], "encoder": [ { "avg": 0.0, "engine_id": 22, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 23, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 24, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 25, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 26, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 27, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 28, "max": 0.0, "min": 0.0, "value": 0.03 } ], "media_enhancement": [ { "avg": 0.0, "engine_id": 38, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 39, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 40, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 41, "max": 0.0, "min": 0.0, "value": 0.05 } ], "render": [] }, "tile_id": 0 }, { "data_list": [ { "avg": 16.09, "max": 30.93, "metrics_type": "XPUM_STATS_POWER", "min": 8.71, "value": 30.81 }, { "metrics_type": "XPUM_STATS_ENERGY", "value": 6951431 }, { "avg": 399, "max": 1400, "metrics_type": "XPUM_STATS_GPU_FREQUENCY", "min": 0, "value": 1150 }, { "avg": 36.87, "max": 43.0, "metrics_type": "XPUM_STATS_GPU_CORE_TEMPERATURE", "min": 36.0, "value": 42.0 }, { "avg": 1002564634, "max": 2311524352, "metrics_type": "XPUM_STATS_MEMORY_USED", "min": 148901888, "value": 2311512064 }, { "avg": 0, "max": 25, "metrics_type": "XPUM_STATS_MEMORY_BANDWIDTH", "min": 0, "value": 0 }, { "avg": 12, "max": 599, "metrics_type": "XPUM_STATS_MEMORY_READ_THROUGHPUT", "min": 12, "value": 13 }, { "avg": 38.4, "max": 100.0, "metrics_type": "XPUM_STATS_GPU_UTILIZATION", "min": 0.0, "value": 100.0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_RESET", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE", "value": 0 }, { "metrics_type": "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE", "value": 0 }, { "avg": 34.98, "max": 39.0, "metrics_type": "XPUM_STATS_MEMORY_TEMPERATURE", "min": 34.0, "value": 38.0 } ], "engine_util": { "3d": [], "compute": [ { "avg": 24.45, "engine_id": 4, "max": 99.77, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 5, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 6, "max": 0.0, "min": 0.0, "value": 0.0 }, { "avg": 0.0, "engine_id": 7, "max": 0.0, "min": 0.0, "value": 0.0 } ], "copy": [ { "avg": 0.0, "engine_id": 37, "max": 9.02, "min": 0.0, "value": 0.04 } ], "decoder": [ { "avg": 0.0, "engine_id": 15, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 16, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 17, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 18, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 19, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 20, "max": 0.0, "min": 0.0, "value": 0.02 }, { "avg": 0.0, "engine_id": 21, "max": 0.0, "min": 0.0, "value": 0.02 } ], "encoder": [ { "avg": 0.0, "engine_id": 29, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 30, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 31, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 32, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 33, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 34, "max": 0.0, "min": 0.0, "value": 0.03 }, { "avg": 0.0, "engine_id": 35, "max": 0.0, "min": 0.0, "value": 0.03 } ], "media_enhancement": [ { "avg": 0.0, "engine_id": 42, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 43, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 44, "max": 0.0, "min": 0.0, "value": 0.05 }, { "avg": 0.0, "engine_id": 45, "max": 0.0, "min": 0.0, "value": 0.05 } ], "render": [] }, "tile_id": 1 } ] } ], "group_id": 1 }

            400:
                description: Error
            500:
                description: Error
    """
    code, message, group_data = stub.getStatisticsByGroupNotForPrometheus(
        groupId)
    if code != 0:
        error = dict(status=code, message=message)
        return jsonify(error), 500
    if "datas" not in group_data:
        return jsonify(group_data)
    for data in group_data["datas"]:
        deviceId = data["device_id"]
        engineCode, engineMsg, engineData = stub.getEngineStatistics(deviceId)
        if engineCode != 0:
            error = dict(message="Error code: {}, error message: {}".format(
                stub.XpumResult(engineCode).name, engineMsg))
            return jsonify(error), 500
        fabricCode, fabricMsg, fabricData = stub.getFabricStatistics(deviceId)
        if fabricCode != 0:
            error_name = stub.XpumResult(fabricCode).name
            if error_name == "XPUM_METRIC_NOT_SUPPORTED" or error_name == "XPUM_METRIC_NOT_ENABLED":
                pass
            else:
                error = dict(message="Error code: {}, error message: {}".format(
                    stub.XpumResult(fabricCode).name, fabricMsg))
                return jsonify(error), 500
        # update fabric throughput data
        if fabricData is not None and "fabric_throughput" in fabricData:
            data["fabric_throughput"] = fabricData["fabric_throughput"]
        # update engine util
        if "engine_util" not in engineData:
            continue
        engineUtilData = engineData["engine_util"]
        if "device_level" in engineUtilData:
            data["engine_util"] = engineUtilData["device_level"]
        if "tile_level" in data:
            for t in data["tile_level"]:
                tileId = t.get("tile_id", None)
                if tileId in engineUtilData:
                    t["engine_util"] = engineUtilData[tileId]
    return jsonify(group_data)
