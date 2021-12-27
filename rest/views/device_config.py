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
    interval = fields.Int(
        metadata={"description": "The interval window value"})

class FrequencySchema(Schema):
    frequency = fields.Int(
        metadata={"description": "The frequency value"})

class SchedulerSchema(Schema):
    scheduler = fields.String(
        metadata={"description": "The scheduler mode"})

class TileConfigSchema(Schema):
    tileId = fields.String(metadata={"description": "Tile id"})
    powerLimit = fields.Nested(PowerLimitSchema)
    powerScope = fields.String(metadata={"description": "power's scope"})
    interval = fields.Nested(IntervalSchema)
    intervalScope = fields.String(metadata={"description": "power's inteval scope"})
    minFreq = fields.Int(metadata={"description": "min frequency"})
    maxFreq = fields.Int(metadata={"description": "max frequency"})
    freqOption = fields.String(metadata={"description": "frequency scope"})
    standby = fields.Nested(StandbySchema)
    standbyOption = fields.String(metadata={"description": "standby option"})
    scheduler = fields.Nested(SchedulerSchema)
    schedulerTimeout = fields.Int(metadata={"description": "scheduler timeout's value"})
    schedulerTimesliceInterval = fields.Int(metadata={"description": "scheduler timeslice's interval value"})
    schedulerTimesliceYieldTimeout = fields.Int(metadata={"description": "scheduler timeslice's yield value"})
class ConfigSchema(Schema):
    deviceId = fields.Int(metadata={"description": "Device id"})
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
                name: standby_mode
                in: body
                description: the standby mode
                schema: StandbySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tile_id
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500

    req = request.get_json()
    if ("tile_id" not in req) or ("standby_mode" not in req):
        return jsonify("json string is invalid"), 500
    tileId = req["tile_id"]
    standby = req["standby_mode"]
    if type(standby) != str:
        return jsonify("Invalid Parameter"), 500
    if type(tileId) != int:
        return jsonify("Invalid Parameter"), 500
    
    if tileId<0:
        return jsonify("invalid Parameter"), 500
    
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
                name: power_limit
                in: body
                description: the power limit value
                schema: PowerLimitSchema
            -
                name: power_average_window
                in: body
                description: the interval window value
                schema: IntervalSchema
            -
                name: device_id
                in: path
                description: Device id
                type: integer
            -
                name: tile_id
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("power_limit" not in req) or ("power_average_window" not in req) or ("tile_id" not in req) :
        return jsonify("json string is invalid"), 500
    power = req["power_limit"]
    interval = req["power_average_window"]
    tileId = req["tile_id"]
    if type(power) != int:
       return jsonify("Invalid Parameter"), 500
    if type(interval) != int:
        return jsonify("Invalid Parameter"), 500
    power = power *1000
    
    if power<0 or interval<0:
        return jsonify("invalid Parameter"), 500
    
    if type(tileId) != int:
        return jsonify("Invalid Parameter"), 500
    
    if tileId<0:
        return jsonify("invalid Parameter"), 500
    
    code, message, data = stub.setPowerLimit(deviceId, tileId, power, interval)
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
                name: min_frequency
                in: body
                description: the min frequency value
                schema: FrequencySchema
            -
                name: max_frequency
                in: body
                description: the max frequency value
                schema: FrequencySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tile_id
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("min_frequency" not in req) or ("max_frequency" not in req):
        return jsonify("json string is invalid"), 500
    tileId = req["tile_id"]
    minFreq = req["min_frequency"]
    maxFreq = req["max_frequency"]
    if type(tileId) != int:
        return jsonify("Invalid Parameter"), 500
    if type(minFreq) != int:
        return jsonify("Invalid Parameter"), 500
    if type(maxFreq) != int:
        return jsonify("Invalid Parameter"), 500
    
    if tileId<0 or minFreq<0 or maxFreq<0 :
        return jsonify("invalid Parameter"), 500
    
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
                name: scheduler_watchdog_timeout
                in: body
                description: parameter of scheduler timeout mode
                type: integer
            -
                name: scheduler_timeslice_interval
                in: body
                description: parameter of scheduler timeslice mode
                type: integer
            -
                name: scheduler_timeslice_yield_timeout
                in: body
                description: the parameter of scheduler timeslice mode
                type: integer
            -
                name: scheduler_mode
                in: body
                description: "the scheduler mode: timeout,timeslice,exclusive mode"
                schema: SchedulerSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
            -
                name: tile_id
                in: body
                description: Tile id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("scheduler_mode" not in req):
        return jsonify("json string is invalid"), 500
    
    tileId = req["tile_id"]
    mode = req["scheduler_mode"]

    if mode.lower() == "timeout":
        if "scheduler_watchdog_timeout" not in req:
            return jsonify("missing parameter \"scheduler_watchdog_timeout\""), 500
        val1 = req["scheduler_watchdog_timeout"]
        val2 = 0
    elif mode.lower() == "timeslice":
        if "scheduler_timeslice_interval" not in req or "scheduler_timeslice_yield_timeout" not in req:
            return jsonify("missing parameter \"scheduler_timeslice_interval\" or \"scheduler_timeslice_yield_timeout\""), 500
        val1 = req["scheduler_timeslice_interval"]
        val2 = req["scheduler_timeslice_yield_timeout"]
    elif mode.lower() == "exclusive":
        val1 = 0
        val2 = 0
    else:
        return jsonify("invalid scheduler mode"), 500
    
    if type(tileId) != int:
        return jsonify("Invalid Parameter"), 500
    if type(mode) != str:
        return jsonify("Invalid Parameter"), 500
    if type(val1) != int:
        return jsonify("Invalid Parameter"), 500
    if type(val2) != int:
        return jsonify("Invalid Parameter"), 500

    if tileId<0 or val1<0 or val2 <0:
        return jsonify("invalid Parameter"), 500
    
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
    get:
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
                name: tile_id
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
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req):
        return jsonify("json string is invalid"), 500
    tileId = req["tile_id"]
    if type(tileId) != int:
        return jsonify("Invalid Parameter"), 500
    if tileId < -1:
        return jsonify("invalid Parameter"), 500
    code, message, data = stub.getConfig(deviceId, tileId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)