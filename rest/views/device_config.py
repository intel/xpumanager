from flask import request, jsonify
import stub
from marshmallow import Schema, fields
class StandbySchema(Schema):
    standby = fields.String(
        metadata={"description": "The standby mode"})

class PowerLimitSchema(Schema):
    powerlimit = fields.Int(
        metadata={"description": "The power limit value"})

class IntervalSchema(Schema):
    Interval = fields.Int(
        metadata={"description": "The interval window value"})

class FrequencySchema(Schema):
    Frequency = fields.Int(
        metadata={"description": "The frequency value"})

class SchedulerSchema(Schema):
    Scheduler = fields.Int(
        metadata={"description": "The scheduler mode"})

class TileConfigSchema(Schema):
    tileId = fields.Int(metadata={"description": "Tile id"})
    minFreq = fields.Int(metadata={"description": "min frequency"})
    maxFreq = fields.Int(metadata={"description": "max frequency"})
    standby = fields.Nested(StandbySchema)
    scheduler = fields.Nested(SchedulerSchema)

class ConfigSchema(Schema):
    deviceId = fields.Int(metadata={"description": "Device id"})
    powerLimit = fields.Nested(PowerLimitSchema)
    interval = fields.Nested(IntervalSchema)
    tileCount = fields.Int()
    tileConfigData = fields.Nested(TileConfigSchema)

def set_standby(deviceId):
    """
    Set standby mode for device
    ---
    put:
        tags:
            - "Config"
        description: Set standby mode for device
        parameters:
            -
                name: standby
                in: body
                description: the standby mode
                schema: StandbySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tileId
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    req = request.get_json()
    tileId = req["tileId"]
    standby = req["standby"]

    if tileId<0 or standby<0:
        return jsonify("invalid parameter"), 500
    
    code, message, data = stub.setStandby(
        deviceId, tileId, standby)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

def set_powerlimit(deviceId):
    """
    Set power limit for device
    ---
    put:
        tags:
            - "Config"
        description: Set power limit for device
        parameters:
            -
                name: powerLimit
                in: body
                description: the power limit value
                schema: PowerLimitSchema
            -
                name: intervalWindow
                in: body
                description: the interval window value
                schema: IntervalSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    req = request.get_json()
    power = req["powerLimit"]*1000
    interval = req["intervalWindow"]
    if power<0 or interval<0:
        return jsonify("invalid parameter"), 500
    
    code, message, data = stub.setPowerLimit(deviceId, power, interval)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

def set_frequencyrange(deviceId):
    """
    Set frequency range for device
    ---
    put:
        tags:
            - "Config"
        description: Set frequency range for device
        parameters:
            -
                name: minFreq
                in: body
                description: the min frequency value
                schema: FrequencySchema
            -
                name: maxFreq
                in: body
                description: the max frequency value
                schema: FrequencySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tileId
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    req = request.get_json()
    tileId = req["tileId"]
    minFreq = req["minFreq"]
    maxFreq = req["maxFreq"]
    
    if tileId<0 or minFreq<0 or maxFreq<0 :
        return jsonify("invalid parameter"), 500
    
    code, message, data = stub.setFrequencyRange(
        deviceId, tileId, minFreq, maxFreq)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

def set_scheduler(deviceId):
    """
    Set scheduler mode for device
    ---
    put:
        tags:
            - "Config"
        description: Set scheduler mode for device
        parameters:
            -
                name: val1
                in: body
                description: parameter of scheduler
                type: integer
            -
                name: val2
                in: body
                description: the parameter of scheduler
                type: integer
            -
                name: mode
                in: body
                description: the scheduler mode
                schema: SchedulerSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tileId
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    req = request.get_json()
    tileId = req["tileId"]
    mode = req["mode"]
    val1 = req["val1"]
    val2 = req["val2"]

    if tileId<0 or mode<0 or val1<0 or val2 <0:
        return jsonify("invalid parameter"), 500
    
    code, message, data = stub.setScheduler(deviceId, tileId, mode, val1, val2)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

def run_reset(deviceId):
    code, message, data = stub.resetDevice(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_config(deviceId):
    """
    Get all configuration for device
    ---
    put:
        tags:
            - "Config"
        description: Get all configuration for device
        parameters:
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tileId
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
                schema: ConfigSchema
            500:
                description: Error
    """
    req = request.get_json()
    tileId = req["tileId"]

    if tileId<0:
        return jsonify("invalid parameter"), 500
    
    code, message, data = stub.getConfig(deviceId, tileId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)