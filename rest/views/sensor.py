#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file sensor.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields, validate, ValidationError

class SensorReadingDataSchema(Schema):
    amc_index = fields.Number(metadata={"description": "AMC index"})
    value = fields.Number(metadata={"description": "Sensor reading value"})
    sensor_high = fields.Number(metadata={"description": "High bound of sensor reading"})
    sensor_low = fields.Number(metadata={"description": "Low bound of sensor reading"})
    sensor_name = fields.Str(metadata={"description": "Sensor name"})
    sensor_unit = fields.Str(metadata={"description": "Sensor unit"})

class SensorReadingSchema(Schema):
    sensor_reading = fields.Nested(SensorReadingDataSchema, many=True, metadata={
                                   "description": "Sensor reading datas"})


def getAMCSensorReading():
    """
    Get sensor reading
    ---
    get:
        tags:
            - "Sensor"
        description: Get sensor reading
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: SensorReadingSchema
            500:
                description: Error
    """
    code, message, data = stub.getAMCSensorReading()
    if code != 0:
        error = dict(message="{}".format(message))
        return jsonify(error), 500
    return jsonify(data)
