from flask import jsonify
import stub
from marshmallow import Schema, fields

class StatisticsDataSchema(Schema):
    value = fields.Int()
    avg = fields.Int()
    max = fields.Int()
    min = fields.Int()

class TileStatisticsSchema(Schema):
    tile_id = fields.Int()
    data_list = fields.Nested(StatisticsDataSchema, many=True)

class StatisticsSchema(Schema):
    device_id = fields.Int()
    begin = fields.Str()
    end = fields.Str()
    device_level = fields.Nested(StatisticsDataSchema, many=True)
    tile_level = fields.Nested(TileStatisticsSchema, many=True)


def get_statistics(deviceId):
    """
    Get statistics by device
    ---
    get:
        description: Get statistics by device
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


def get_group_statistics(groupId):
    code, message, data = stub.getStatisticsByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
