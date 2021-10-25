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
            500:
                description: Error
    """
    code, message, data = stub.getStatistics(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


class GroupStatisticsSchema(Schema):
    group_id = fields.Int(metadata={"description": "Group id"})
    datas = fields.Nested(StatisticsSchema, many=True, metadata={
                          "description": "Statistics data for devices in group"})


def get_group_statistics(groupId):
    """Get statistics by group
    ---
    get:
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
            400:
                description: Error
            500:
                description: Error
    """
    code, message, data = stub.getStatisticsByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
